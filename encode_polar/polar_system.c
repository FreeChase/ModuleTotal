#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define PI 3.14159265358979323846
#define LIST_SIZE 8       // SCL 的列表大小，提升到 8 以获取最佳性能
#define CRC_POLY 0x1021   // CRC-16-CCITT 多项式

// ==========================================
// 1. 核心上下文结构体与内存池设计 (支持 SCL)
// ==========================================
typedef struct {
    int N;             // 物理总长 (如 1024)
    int K;             // 信息位长度 (Payload + CRC, 如 512)
    int n_stages;      // 树的深度 log2(N)
    int *frozen_flag;  // 1 为冻结位(填0)，0 为信息位(放数据)
    
    // SCL 多路径状态
    int active_paths;
    double path_metric[LIST_SIZE];
    
    // SCL 独立内存池：[list_index][stage][N]
    double ***LLR;   
    int ***C;        
    int path_u_est[LIST_SIZE][2048]; // 记录每条路径的完整判决结果
} PolarContext;

void init_polar_system(PolarContext *ctx, int N, int K) {
    ctx->N = N;
    ctx->K = K;
    ctx->n_stages = (int)log2(N);
    ctx->frozen_flag = (int *)malloc(N * sizeof(int));
    
    // 分配 SCL 多路径内存
    ctx->LLR = (double ***)malloc(LIST_SIZE * sizeof(double **));
    ctx->C = (int ***)malloc(LIST_SIZE * sizeof(int **));
    for (int l = 0; l < LIST_SIZE; l++) {
        ctx->LLR[l] = (double **)malloc((ctx->n_stages + 1) * sizeof(double *));
        ctx->C[l] = (int **)malloc((ctx->n_stages + 1) * sizeof(int *));
        for (int s = 0; s <= ctx->n_stages; s++) {
            ctx->LLR[l][s] = (double *)calloc(N, sizeof(double));
            ctx->C[l][s] = (int *)calloc(N, sizeof(int));
        }
    }
    
    // 模拟极化权重计算 (此处简化的启发式算法)
    double *scores = (double *)calloc(N, sizeof(double));
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < ctx->n_stages; j++) {
            if ((i >> j) & 1) scores[i] += pow(1.25, j);
        }
        ctx->frozen_flag[i] = 1; 
    }
    
    for (int k = 0; k < K; k++) {
        double max_score = -1.0;
        int max_idx = 0;
        for (int i = 0; i < N; i++) {
            if (ctx->frozen_flag[i] == 1 && scores[i] > max_score) {
                max_score = scores[i];
                max_idx = i;
            }
        }
        ctx->frozen_flag[max_idx] = 0; 
    }
    free(scores);
}

// ==========================================
// 2. CRC 与 发射机编码
// ==========================================
void append_crc(const int *payload, int payload_len, int *output_msg) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < payload_len; i++) {
        output_msg[i] = payload[i];
        crc ^= (payload[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ CRC_POLY;
            else crc <<= 1;
        }
    }
    for (int i = 0; i < 16; i++) {
        output_msg[payload_len + i] = (crc >> (15 - i)) & 1;
    }
}

int check_crc(const int *msg, int total_len) {
    uint16_t crc = 0xFFFF;
    int payload_len = total_len - 16;
    for (int i = 0; i < payload_len; i++) {
        crc ^= (msg[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ CRC_POLY;
            else crc <<= 1;
        }
    }
    for (int i = 0; i < 16; i++) {
        int bit = (crc >> (15 - i)) & 1;
        if (msg[payload_len + i] != bit) return 0; // 校验失败
    }
    return 1; // 校验成功
}

void polar_encode(PolarContext *ctx, const int *message, int *x) {
    int *u = (int *)calloc(ctx->N, sizeof(int));
    int msg_idx = 0;
    for (int i = 0; i < ctx->N; i++) {
        u[i] = (ctx->frozen_flag[i] == 1) ? 0 : message[msg_idx++];
    }
    memcpy(x, u, ctx->N * sizeof(int));
    for (int stage = 0; stage < ctx->n_stages; stage++) {
        int step = 1 << stage;
        for (int i = 0; i < ctx->N; i += 2 * step) {
            for (int j = 0; j < step; j++) {
                x[i + j] = x[i + j] ^ x[i + j + step];
            }
        }
    }
    free(u);
}

// ==========================================
// 3. 物理信道
// ==========================================
double generate_gaussian_noise(double std_dev) {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    if (u1 <= 1e-7) u1 = 1e-7; 
    return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2) * std_dev;
}

// ==========================================
// 4. 接收机：CA-SCL 译码器
// ==========================================
typedef struct { int src_idx; int bit_val; double pm; } PathCandidate;

int compare_pm(const void *a, const void *b) {
    double diff = ((PathCandidate *)a)->pm - ((PathCandidate *)b)->pm;
    return (diff > 0) - (diff < 0);
}

void ca_scl_decode(PolarContext *ctx, double *rx_llr, int *best_message) {
    int n = ctx->n_stages;
    ctx->active_paths = 1;
    ctx->path_metric[0] = 0.0;
    memcpy(ctx->LLR[0][n], rx_llr, ctx->N * sizeof(double));

    for (int phi = 0; phi < ctx->N; phi++) {
        int start_stage = (phi == 0) ? n : 0;
        if (phi != 0) {
            int diff = phi ^ (phi - 1);
            while (diff > 0) { start_stage++; diff >>= 1; }
        }

        // 1. Down-pass 计算 LLR (归一化 Min-Sum)
        for (int l = 0; l < ctx->active_paths; l++) {
            for (int s = start_stage; s >= 1; s--) {
                int length = 1 << s, half = length / 2;
                int is_right = (phi >> (s - 1)) & 1;
                if (!is_right) {
                    for (int i = 0; i < half; i++) {
                        double a = ctx->LLR[l][s][i], b = ctx->LLR[l][s][i + half];
                        ctx->LLR[l][s - 1][i] = (a * b > 0 ? 1.0 : -1.0) * 0.75 * fmin(fabs(a), fabs(b)); 
                    }
                } else {
                    for (int i = 0; i < half; i++) {
                        double a = ctx->LLR[l][s][i], b = ctx->LLR[l][s][i + half];
                        ctx->LLR[l][s - 1][i] = b + ((ctx->C[l][s][i] == 0) ? a : -a);
                    }
                }
            }
        }

        // 2. 叶子节点判决与路径分裂 (引入深拷贝安全修复)
        if (ctx->frozen_flag[phi] == 1) {
            for (int l = 0; l < ctx->active_paths; l++) {
                ctx->C[l][0][0] = 0;
                if (ctx->LLR[l][0][0] < 0) ctx->path_metric[l] += fabs(ctx->LLR[l][0][0]);
                ctx->path_u_est[l][phi] = 0;
            }
        } else {
            PathCandidate candidates[LIST_SIZE * 2];
            int cand_count = 0;
            
            for (int l = 0; l < ctx->active_paths; l++) {
                double llr = ctx->LLR[l][0][0];
                candidates[cand_count++] = (PathCandidate){l, 0, ctx->path_metric[l] + (llr < 0 ? fabs(llr) : 0)};
                candidates[cand_count++] = (PathCandidate){l, 1, ctx->path_metric[l] + (llr > 0 ? fabs(llr) : 0)};
            }
            
            if (cand_count > LIST_SIZE) {
                qsort(candidates, cand_count, sizeof(PathCandidate), compare_pm);
                cand_count = LIST_SIZE; 
            }
            
            int old_active = ctx->active_paths;

            // [安全修复版] 动态分配临时备份空间 (深拷贝)，彻底解决路径交叉覆写 Bug
            double ***backup_LLR = (double ***)malloc(old_active * sizeof(double **));
            int ***backup_C = (int ***)malloc(old_active * sizeof(int **));
            int backup_path_u_est[LIST_SIZE][2048];
            
            for (int l = 0; l < old_active; l++) {
                backup_LLR[l] = (double **)malloc((ctx->n_stages + 1) * sizeof(double *));
                backup_C[l] = (int **)malloc((ctx->n_stages + 1) * sizeof(int *));
                for (int s = 0; s <= ctx->n_stages; s++) {
                    backup_LLR[l][s] = (double *)malloc(ctx->N * sizeof(double));
                    backup_C[l][s] = (int *)malloc(ctx->N * sizeof(int));
                    memcpy(backup_LLR[l][s], ctx->LLR[l][s], ctx->N * sizeof(double));
                    memcpy(backup_C[l][s], ctx->C[l][s], ctx->N * sizeof(int));
                }
                memcpy(backup_path_u_est[l], ctx->path_u_est[l], ctx->N * sizeof(int));
            }

            // 从安全的 backup 中覆写回工作区
            for (int i = 0; i < cand_count; i++) {
                int src = candidates[i].src_idx;
                
                // 恢复源路径的历史状态
                for (int s = 0; s <= ctx->n_stages; s++) {
                    memcpy(ctx->LLR[i][s], backup_LLR[src][s], ctx->N * sizeof(double));
                    memcpy(ctx->C[i][s], backup_C[src][s], ctx->N * sizeof(int));
                }
                memcpy(ctx->path_u_est[i], backup_path_u_est[src], ctx->N * sizeof(int));
                
                // 更新当前节点的新状态
                ctx->path_metric[i] = candidates[i].pm;
                ctx->C[i][0][0] = candidates[i].bit_val;
                ctx->path_u_est[i][phi] = candidates[i].bit_val;
            }
            ctx->active_paths = cand_count;

            // 释放备份内存
            for (int l = 0; l < old_active; l++) {
                for (int s = 0; s <= ctx->n_stages; s++) {
                    free(backup_LLR[l][s]); free(backup_C[l][s]);
                }
                free(backup_LLR[l]); free(backup_C[l]);
            }
            free(backup_LLR); free(backup_C);
        }

        // 3. Up-pass 向上汇总
        int temp_phi = phi;
        for (int l = 0; l < ctx->active_paths; l++) {
            int tp = temp_phi;
            for (int s = 0; s < n; s++) {
                if (!(tp & 1)) {
                    for (int i = 0; i < (1 << s); i++) ctx->C[l][s + 1][i] = ctx->C[l][s][i];
                    break;
                } else {
                    for (int i = 0; i < (1 << s); i++) {
                        ctx->C[l][s + 1][i] = ctx->C[l][s + 1][i] ^ ctx->C[l][s][i];
                        ctx->C[l][s + 1][i + (1 << s)] = ctx->C[l][s][i];
                    }
                    tp >>= 1;
                }
            }
        }
    }

    // 4. CRC 校验筛选最优路径
    int best_idx = 0;
    double min_pm = 1e9;
    int crc_passed = 0;
    
    for (int l = 0; l < ctx->active_paths; l++) {
        int extracted_msg[1024];
        int idx = 0;
        for (int i = 0; i < ctx->N; i++) {
            if (ctx->frozen_flag[i] == 0) extracted_msg[idx++] = ctx->path_u_est[l][i];
        }
        
        if (check_crc(extracted_msg, ctx->K)) {
            crc_passed = 1;
            if (ctx->path_metric[l] < min_pm) {
                min_pm = ctx->path_metric[l];
                best_idx = l;
            }
        }
    }
    
    // 降级保护
    if (!crc_passed) {
        min_pm = 1e9;
        for (int l = 0; l < ctx->active_paths; l++) {
            if (ctx->path_metric[l] < min_pm) {
                min_pm = ctx->path_metric[l]; best_idx = l;
            }
        }
        printf("   -> 警告：所有路径均未通过 CRC，强制输出最有可能的一条。\n");
    }

    // 提取最优信息
    int idx = 0;
    for (int i = 0; i < ctx->N; i++) {
        if (ctx->frozen_flag[i] == 0) best_message[idx++] = ctx->path_u_est[best_idx][i];
    }
}

// ==========================================
// 5. 主程序端到端验证
// ==========================================
int main() {
    srand((unsigned)time(NULL));
    int N = 1024;
    int payload_len = 496;
    int crc_len = 16;
    int K = payload_len + crc_len; 

    printf("=== Polar 码 (N=%d, Payload=%d, CRC=%d) CA-SCL仿真 ===\n\n", N, payload_len, crc_len);

    PolarContext engine;
    init_polar_system(&engine, N, K);

    int *raw_payload   = (int *)malloc(payload_len * sizeof(int));
    int *msg_with_crc  = (int *)malloc(K * sizeof(int));
    int *x             = (int *)malloc(N * sizeof(int));
    double *llr        = (double *)malloc(N * sizeof(double));
    int *decoded_msg   = (int *)malloc(K * sizeof(int));

    // 测试：极高噪声环境下的纯随机数据
    for (int i = 0; i < payload_len; i++) raw_payload[i] = rand() % 2;
    append_crc(raw_payload, payload_len, msg_with_crc);
    polar_encode(&engine, msg_with_crc, x);

    // 设置为 0.75，在此信噪比下基础 SC 算法已无法存活，但 CA-SCL 可以完美纠错
    double noise_std_dev = 0.80; 
    for (int i = 0; i < N; i++) {
        double y = ((x[i] == 0) ? 1.0 : -1.0) + generate_gaussian_noise(noise_std_dev);
        llr[i] = 2.0 * y / (noise_std_dev * noise_std_dev);
    }
    printf("[1/3] 信道：AWGN (Standard Dev = %.2f)\n", noise_std_dev);
    printf("[2/3] 接收：CA-SCL 译码 (List Size = %d)...\n", LIST_SIZE);
    
    ca_scl_decode(&engine, llr, decoded_msg);

    int errors = 0;
    for (int i = 0; i < payload_len; i++) {
        if (decoded_msg[i] != raw_payload[i]) errors++;
    }
    printf("[3/3] 统计：纯随机数据载荷中，解码错误 %d 位。\n", errors);
    if (errors == 0) printf(">>> 结论：完美纠错！这才是真正的 5G 标准威力！\n");

    // 释放内存
    free(raw_payload); free(msg_with_crc); free(x); free(llr); free(decoded_msg);
    free(engine.frozen_flag);
    for (int l = 0; l < LIST_SIZE; l++) {
        for (int s = 0; s <= engine.n_stages; s++) {
            free(engine.LLR[l][s]); free(engine.C[l][s]);
        }
        free(engine.LLR[l]); free(engine.C[l]);
    }
    free(engine.LLR); free(engine.C);
    
    return 0;
}
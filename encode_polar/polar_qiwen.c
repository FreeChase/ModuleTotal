#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define PI 3.14159265358979323846
#define LIST_SIZE 8
#define CRC_POLY 0x1021

#define MAX_N 1024
#define MAX_STAGES 10

// ==========================================
// 1. 核心上下文结构体
// ==========================================
typedef struct {
    int N;             
    int K;             
    int n_stages;      
    int frozen_flag[MAX_N];  
    
    int active_paths;
    double path_metric[LIST_SIZE];
    
    // LLR[l][stage][index] - stage 0是叶子，stage n是信道
    double LLR[LIST_SIZE][MAX_STAGES + 1][MAX_N];
    // C[l][stage][index] - 部分和
    int C[LIST_SIZE][MAX_STAGES + 1][MAX_N];
    // 路径估计比特
    int path_u_est[LIST_SIZE][MAX_N]; 

    // 备份数组 (用于路径分裂时的深拷贝)
    double backup_LLR[LIST_SIZE][MAX_STAGES + 1][MAX_N];
    int backup_C[LIST_SIZE][MAX_STAGES + 1][MAX_N];
    int backup_path_u_est[LIST_SIZE][MAX_N];
} PolarContext;

// ==========================================
// 2. 冻结位生成 (修复为：极化权重法 PW)
// ==========================================
void init_polar_system(PolarContext *ctx, int N, int K) {
    if (N > MAX_N) {
        printf("Error: N exceeds MAX_N.\n");
        exit(1);
    }
    
    memset(ctx, 0, sizeof(PolarContext));
    
    ctx->N = N;
    ctx->K = K;
    ctx->n_stages = (int)log2(N);
    
    // 使用 PW (Polarization Weight) 算法计算可靠度，避免汉明重量的严重冲突
    double scores[MAX_N] = {0};
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < ctx->n_stages; j++) {
            if ((i >> j) & 1) scores[i] += pow(1.25, j); 
        }
        ctx->frozen_flag[i] = 1; 
    }
    
    // 选出得分最高的 K 个信道作为信息位
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
}

// ==========================================
// 3. CRC
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
        if (msg[payload_len + i] != bit) return 0; 
    }
    return 1; 
}

// ==========================================
// 4. Polar 编码
// ==========================================
void polar_encode(PolarContext *ctx, const int *message, int *x) {
    int u[MAX_N] = {0};
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
}

// ==========================================
// 5. AWGN 信道
// ==========================================
double generate_gaussian_noise(double std_dev) {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    if (u1 <= 1e-7) u1 = 1e-7; 
    return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2) * std_dev;
}

// ==========================================
// 6. LLR 计算函数 (标准F/G函数)
// ==========================================
double f_func(double a, double b) {
    double sign = (a * b >= 0) ? 1.0 : -1.0;
    double min_abs = fmin(fabs(a), fabs(b));
    return sign * min_abs;
}

double g_func(double a, double b, int u) {
    return b + ((u == 0) ? a : -a);
}

// ==========================================
// 7. CA-SCL 译码器 (修复了块更新逻辑)
// ==========================================
typedef struct { 
    int src_idx; 
    int bit_val; 
    double pm; 
} PathCandidate;

int compare_pm(const void *a, const void *b) {
    double diff = ((PathCandidate *)a)->pm - ((PathCandidate *)b)->pm;
    if (fabs(diff) < 1e-9) return 0;
    return (diff > 0) ? 1 : -1;
}

void ca_scl_decode(PolarContext *ctx, double *rx_llr, int *best_message) {
    int n = ctx->n_stages;
    
    ctx->active_paths = 1;
    ctx->path_metric[0] = 0.0;
    
    for (int l = 0; l < LIST_SIZE; l++) {
        memcpy(ctx->LLR[l][n], rx_llr, ctx->N * sizeof(double));
        memset(ctx->C[l], 0, (MAX_STAGES + 1) * MAX_N * sizeof(int));
        memset(ctx->path_u_est[l], 0, MAX_N * sizeof(int));
        if (l > 0) ctx->path_metric[l] = 1e9;
    }
    
    for (int phi = 0; phi < ctx->N; phi++) {
        
        // 1. Down-pass (批量计算当前所需的所有 LLR)
        int start_stage = (phi == 0) ? n : 0;
        if (phi != 0) {
            int diff = phi ^ (phi - 1);
            while (diff > 0) { start_stage++; diff >>= 1; }
        }

        for (int l = 0; l < ctx->active_paths; l++) {
            for (int s = start_stage; s >= 1; s--) {
                int half = 1 << (s - 1);
                int is_right = (phi >> (s - 1)) & 1;

                if (!is_right) {
                    for (int i = 0; i < half; i++) {
                        ctx->LLR[l][s - 1][i] = f_func(ctx->LLR[l][s][i], ctx->LLR[l][s][i + half]);
                    }
                } else {
                    for (int i = 0; i < half; i++) {
                        ctx->LLR[l][s - 1][i] = g_func(ctx->LLR[l][s][i], ctx->LLR[l][s][i + half], ctx->C[l][s][i]);
                    }
                }
            }
        }
        
        // 2. 叶子节点判决与路径分裂
        if (ctx->frozen_flag[phi] == 1) {
            for (int l = 0; l < ctx->active_paths; l++) {
                double llr = ctx->LLR[l][0][0];
                if (llr < 0) ctx->path_metric[l] += fabs(llr);
                ctx->path_u_est[l][phi] = 0;
                ctx->C[l][0][0] = 0;
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
            
            // 集中备份
            memcpy(ctx->backup_LLR, ctx->LLR, old_active * (MAX_STAGES + 1) * MAX_N * sizeof(double));
            memcpy(ctx->backup_C, ctx->C, old_active * (MAX_STAGES + 1) * MAX_N * sizeof(int));
            memcpy(ctx->backup_path_u_est, ctx->path_u_est, old_active * MAX_N * sizeof(int));
            
            // 覆写分配
            for (int i = 0; i < cand_count; i++) {
                int src = candidates[i].src_idx;
                int bit = candidates[i].bit_val;
                
                memcpy(ctx->LLR[i], ctx->backup_LLR[src], (MAX_STAGES + 1) * MAX_N * sizeof(double));
                memcpy(ctx->C[i], ctx->backup_C[src], (MAX_STAGES + 1) * MAX_N * sizeof(int));
                memcpy(ctx->path_u_est[i], ctx->backup_path_u_est[src], MAX_N * sizeof(int));
                
                ctx->path_metric[i] = candidates[i].pm;
                ctx->path_u_est[i][phi] = bit;
                ctx->C[i][0][0] = bit;
            }
            ctx->active_paths = cand_count;
        }

        // 3. Up-pass (部分和矩阵向上逐级合并传递)
        for (int l = 0; l < ctx->active_paths; l++) {
            int tp = phi;
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
    
    // 4. CRC 校验筛选
    int best_idx = 0;
    double min_pm = 1e9;
    int crc_passed = 0;
    
    for (int l = 0; l < ctx->active_paths; l++) {
        int extracted_msg[MAX_N];
        int idx = 0;
        for (int i = 0; i < ctx->N; i++) {
            if (ctx->frozen_flag[i] == 0) {
                extracted_msg[idx++] = ctx->path_u_est[l][i];
            }
        }
        
        if (check_crc(extracted_msg, ctx->K)) {
            crc_passed = 1;
            if (ctx->path_metric[l] < min_pm) {
                min_pm = ctx->path_metric[l];
                best_idx = l;
            }
        }
    }
    
    if (!crc_passed) {
        min_pm = 1e9;
        for (int l = 0; l < ctx->active_paths; l++) {
            if (ctx->path_metric[l] < min_pm) {
                min_pm = ctx->path_metric[l];
                best_idx = l;
            }
        }
    }
    
    int idx = 0;
    for (int i = 0; i < ctx->N; i++) {
        if (ctx->frozen_flag[i] == 0) {
            best_message[idx++] = ctx->path_u_est[best_idx][i];
        }
    }
}

// ==========================================
// 8. 主程序仿真
// ==========================================
int main() {
    srand((unsigned)time(NULL));
    
    int N = 1024;
    int payload_len = 496;
    int crc_len = 16;
    int K = payload_len + crc_len; 

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║     Polar 码 CA-SCL 译码器 (已修复版)            ║\n");
    printf("║     N=%d, K=%d, List=%d, CRC=%d                  ║\n", N, K, LIST_SIZE, crc_len);
    printf("╚══════════════════════════════════════════════════╝\n\n");

    PolarContext *engine = (PolarContext *)malloc(sizeof(PolarContext));
    if (engine == NULL) {
        printf("Memory allocation failed!\n");
        return -1;
    }

    int *raw_payload   = (int *)malloc(payload_len * sizeof(int));
    int *msg_with_crc  = (int *)malloc(K * sizeof(int));
    int *x             = (int *)malloc(N * sizeof(int));
    double *llr        = (double *)malloc(N * sizeof(double));
    int *decoded_msg   = (int *)malloc(K * sizeof(int));

    double snr_db_list[] = {1.5, 2.0, 2.5, 3.0, 3.5, 4.0};
    int num_snr = sizeof(snr_db_list) / sizeof(snr_db_list[0]);
    int frames_per_snr = 100;

    printf("开始多SNR性能测试 (每点 %d 帧)...\n\n", frames_per_snr);
    printf("%-8s %-10s %-10s %-10s\n", "SNR(dB)", "错误帧", "错误比特", "BER");
    printf("────────────────────────────────────────\n");

    for (int snr_idx = 0; snr_idx < num_snr; snr_idx++) {
        double snr_db = snr_db_list[snr_idx];
        
        init_polar_system(engine, N, K);

        int frame_errors = 0;
        int total_bit_errors = 0;

        for (int frame = 0; frame < frames_per_snr; frame++) {
            for (int i = 0; i < payload_len; i++) {
                raw_payload[i] = rand() % 2;
            }
            
            append_crc(raw_payload, payload_len, msg_with_crc);
            polar_encode(engine, msg_with_crc, x);

            double noise_std = pow(10, -snr_db / 20.0);
            for (int i = 0; i < N; i++) {
                double y = ((x[i] == 0) ? 1.0 : -1.0) + generate_gaussian_noise(noise_std);
                llr[i] = 2.0 * y / (noise_std * noise_std);
            }

            ca_scl_decode(engine, llr, decoded_msg);

            int bit_errors = 0;
            for (int i = 0; i < payload_len; i++) {
                if (decoded_msg[i] != raw_payload[i]) {
                    bit_errors++;
                }
            }
            
            if (bit_errors > 0) {
                frame_errors++;
            }
            total_bit_errors += bit_errors;
        }

        double ber = (double)total_bit_errors / (frames_per_snr * payload_len);
        
        printf("%-8.1f %-10d %-10d %-10.6f\n", 
               snr_db, frame_errors, total_bit_errors, ber);
    }

    printf("────────────────────────────────────────\n");
    printf("测试完成!\n\n");

    free(raw_payload);
    free(msg_with_crc);
    free(x);
    free(llr);
    free(decoded_msg);
    free(engine); 
    
    return 0;
}
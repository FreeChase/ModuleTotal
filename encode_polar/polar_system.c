#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define PI 3.14159265358979323846

// ==========================================
// 1. 核心上下文结构体与内存池设计
// ==========================================
typedef struct {
    int N;             // 物理总长 (如 1024)
    int K;             // 信息位长度 (如 512)
    int n_stages;      // 树的深度 log2(N)
    int *frozen_flag;  // 1 为冻结位(填0)，0 为信息位(放数据)
    
    // 全局内存池与滑窗指针
    double *llr_pool;  
    int *c_pool;       
    double *LLR[20];   
    int *C[20];        
} PolarContext;

// ==========================================
// 2. 初始化与极化信道评估 (DNA 设定)
// ==========================================
void init_polar_system(PolarContext *ctx, int N, int K) {
    ctx->N = N;
    ctx->K = K;
    ctx->n_stages = (int)log2(N);
    
    ctx->frozen_flag = (int *)malloc(N * sizeof(int));
    int pool_size = (ctx->n_stages + 1) * N; 
    ctx->llr_pool = (double *)malloc(pool_size * sizeof(double));
    ctx->c_pool   = (int *)malloc(pool_size * sizeof(int));
    
    for (int s = 0; s <= ctx->n_stages; s++) {
        ctx->LLR[s] = ctx->llr_pool + (s * N);
        ctx->C[s]   = ctx->c_pool   + (s * N);
    }
    
    // 模拟 5G 极化权重计算 (高位权重法)
    double *scores = (double *)malloc(N * sizeof(double));
    for (int i = 0; i < N; i++) {
        scores[i] = 0;
        for (int j = 0; j < ctx->n_stages; j++) {
            if ((i >> j) & 1) scores[i] += pow(1.25, j);
        }
        ctx->frozen_flag[i] = 1; // 默认全部冻结
    }
    
    // 选出得分最高的 K 个信道作为“富人信道”
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
// 3. 发射机：信息映射与极速蝴蝶编码
// ==========================================
void polar_encode(PolarContext *ctx, int *message, int *x) {
    int *u = (int *)malloc(ctx->N * sizeof(int));
    
    // 映射：将 message 填入信息位，冻结位填 0
    int msg_idx = 0;
    for (int i = 0; i < ctx->N; i++) {
        u[i] = (ctx->frozen_flag[i] == 1) ? 0 : message[msg_idx++];
    }

    // O(N log N) 蝴蝶网络异或编码
    memcpy(x, u, ctx->N * sizeof(int));
    for(int stage = 0; stage < ctx->n_stages; stage++) {
        int step = 1 << stage;
        for(int i = 0; i < ctx->N; i += 2 * step) {
            for(int j = 0; j < step; j++) {
                x[i + j] = x[i + j] ^ x[i + j + step];
            }
        }
    }
    free(u);
}

// ==========================================
// 4. 物理信道：AWGN 噪声生成器 (Box-Muller 变换)
// ==========================================
double generate_gaussian_noise(double std_dev) {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    // 防止 log(0)
    if (u1 <= 1e-7) u1 = 1e-7; 
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
    return z0 * std_dev;
}

// ==========================================
// 5. 接收机：非递归 SC 内存池译码器
// ==========================================
// 注：为保证完整运行的极度稳定，这里采用自底向上的层级状态机
void sc_decode_fast(PolarContext *ctx, double *rx_llr, int *u_est) {
    int n = ctx->n_stages;
    memcpy(ctx->LLR[n], rx_llr, ctx->N * sizeof(double));

    for (int phi = 0; phi < ctx->N; phi++) {
        // 寻找分叉点，决定向回退几级重新计算
        int start_stage = 0;
        if (phi == 0) start_stage = n;
        else {
            int diff = phi ^ (phi - 1);
            while (diff > 0) { start_stage++; diff >>= 1; }
        }

        // Down-pass (下沉计算 LLR)
        for (int s = start_stage; s >= 1; s--) {
            int length = 1 << s;
            int half = length / 2;
            int is_right = (phi >> (s - 1)) & 1;

            if (!is_right) { // 左支 f 节点
                for (int i = 0; i < half; i++) {
                    double a = ctx->LLR[s][i], b = ctx->LLR[s][i + half];
                    ctx->LLR[s - 1][i] = (a * b > 0 ? 1.0 : -1.0) * fmin(fabs(a), fabs(b));
                }
            } else { // 右支 g 节点
                for (int i = 0; i < half; i++) {
                    double a = ctx->LLR[s][i], b = ctx->LLR[s][i + half];
                    int u_left = ctx->C[s][i];
                    ctx->LLR[s - 1][i] = b + ((u_left == 0) ? a : -a);
                }
            }
        }

        // 叶子节点判决
        ctx->C[0][0] = (ctx->frozen_flag[phi] == 1) ? 0 : ((ctx->LLR[0][0] < 0) ? 1 : 0);
        u_est[phi] = ctx->C[0][0];

        // Up-pass (部分和向上汇总)
        int temp_phi = phi;
        for (int s = 0; s < n; s++) {
            if (!(temp_phi & 1)) {
                for (int i = 0; i < (1 << s); i++) ctx->C[s + 1][i] = ctx->C[s][i];
                break; 
            } else {
                for (int i = 0; i < (1 << s); i++) {
                    ctx->C[s + 1][i] = ctx->C[s + 1][i] ^ ctx->C[s][i];
                    ctx->C[s + 1][i + (1 << s)] = ctx->C[s][i];
                }
                temp_phi >>= 1;
            }
        }
    }
}

// ==========================================
// 6. 主程序：端到端业务流仿真
// ==========================================
int main() {
    srand(time(NULL));
    int N = 1024, K = 512;
    printf("=== Polar Code (N=%d, K=%d) 端到端仿真 ===\n\n", N, K);

    // 1. 初始化引擎
    PolarContext engine;
    init_polar_system(&engine, N, K);
    
    // 申请业务缓存
    int *message = (int *)malloc(K * sizeof(int));
    int *x       = (int *)malloc(N * sizeof(int));
    double *y    = (double *)malloc(N * sizeof(double));
    double *llr  = (double *)malloc(N * sizeof(double));
    int *u_est   = (int *)malloc(N * sizeof(int));

    // 2. 生成随机原始数据包
    for (int i = 0; i < K; i++) message[i] = rand() % 2;
    printf("[1/5] 业务层: 随机生成 %d bit 原始信息完毕。\n", K);

    // 3. 发射机：Polar 编码
    polar_encode(&engine, message, x);
    printf("[2/5] 发射机: Polar 编码完毕，输出 %d bit 物理码字。\n", N);

    // 4. 物理信道：调制与加性高斯白噪声
    // 假设设定一个比较恶劣的信道，噪声标准差为 0.8
    double noise_std_dev = 0.8; 
    for (int i = 0; i < N; i++) {
        double signal = (x[i] == 0) ? 1.0 : -1.0; // BPSK 调制
        y[i] = signal + generate_gaussian_noise(noise_std_dev);
        // 接收机计算 LLR (在 AWGN 下，LLR 约等于 2 * y / sigma^2)
        llr[i] = 2.0 * y[i] / (noise_std_dev * noise_std_dev);
    }
    printf("[3/5] 信  道: BPSK 调制并叠加 AWGN 噪声 (标准差 %.2f)。\n", noise_std_dev);

    // 5. 接收机：极速译码
    sc_decode_fast(&engine, llr, u_est);
    printf("[4/5] 接收机: 非递归 SC 内存池极速译码完毕。\n");

    // 6. 验证误码率 (BER)
    int errors = 0;
    int msg_idx = 0;
    for (int i = 0; i < N; i++) {
        if (engine.frozen_flag[i] == 0) { // 只比对我们关心的信息位
            if (u_est[i] != message[msg_idx]) errors++;
            msg_idx++;
        }
    }
    
    printf("[5/5] 统  计: %d 个信息位中，解码错误 %d 位。\n", K, errors);
    if (errors == 0) {
        printf("\n>>> 结论: 完美传输！Polar 码成功抗住了严重的高斯噪声！\n");
    } else {
        printf("\n>>> 结论: 出现误码。噪声过大击穿了纠错极限，建议加入 SCL 和 CRC 拯救！\n");
    }

    // 释放所有内存
    free(message); free(x); free(y); free(llr); free(u_est);
    free(engine.frozen_flag); free(engine.llr_pool); free(engine.c_pool);
    return 0;
}
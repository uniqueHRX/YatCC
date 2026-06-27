#include <sysy/sylib.h>
#define N 1024

int a[N][N];
int b[N][N];
int c[N][N];
int d[N][N];

/* 
 * 核心挑战：多个循环嵌套，内层有大量重复计算
 * 最佳策略：
 *   1. Mem2Reg 消除 alloca
 *   2. Inliner 内联改进版 mm
 *   3. CP+CF 将 1024*20 等常量预计算
 *   4. LICM 将循环不变的 load 外提
 *   5. LoopUnrolling 展开小循环  
 *   6. CSE 消除展开后的冗余
 *   7. DCE 清除死代码
 * 默认顺序（Mem2Reg→Inliner→LICM→Unroll→CP→CF→CSE→DCE）的缺陷：
 *   CP/CF 在 LICM 之后，loop invariants 无法被检测
 *   Unroll 后不再次 CSE，冗余代码残留
 */
void compute(int n){
    int i = 0;
    while (i < n){
        int j = 0;
        while (j < n){
            int k = 0;
            int sum = 0;
            int tmp = 0;
            while (k < n){
                int aik = a[i][k];
                int bkj = b[k][j];
                /* 重复计算让 CSE 发挥作用 */
                int t1 = aik * bkj;
                int t2 = aik * bkj;
                int t3 = aik * bkj;
                /* 代数恒等式，AlgebraicIdentities 可简化 */
                int t4 = t1 * 1;
                int t5 = t2 + 0;
                int t6 = t3 / 1;
                /* 强度削减 */
                sum = sum + t4 + t5 + t6;
                k = k + 1;
            }
            /* 循环不变量：n 不变，c[i][j] 可在循环外计算一次 */
            c[i][j] = sum;
            d[i][j] = sum + n * 2;
            j = j + 1;
        }
        i = i + 1;
    }
}

int main(){
    int n = getint();
    int i = 0;
    while (i < N){
        int j = 0;
        while (j < N){
            a[i][j] = (i + j) % 100;
            b[i][j] = (i * j) % 100;
            j = j + 1;
        }
        i = i + 1;
    }
    starttime();
    compute(n);
    stoptime();
    int ans = 0;
    i = 0;
    while (i < N){
        int j = 0;
        while (j < N){
            ans = ans + c[i][j] + d[i][j];
            j = j + 1;
        }
        i = i + 1;
    }
    putint(ans);
    putch(10);
    return 0;
}
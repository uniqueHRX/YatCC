#include <sysy/sylib.h>
#define N 400

int a[N][N];
int b[N][N];
int c[N];

/*
 * 挑战：强度削减 + 循环展开 + CSE 的三重交互
 * 
 * 最佳顺序：
 *   1. Mem2Reg — 消除 alloca
 *   2. StrengthReduction — 将 i*5 等乘除运算转为移位/加法
 *   3. ConstantPropagation — 传播可推导的常量
 *   4. ConstantFolding — 折叠常量表达式
 *   5. LICM — 外提循环不变的地址计算
 *   6. LoopUnrolling — 展开内层小循环
 *   7. CSE — 消除展开后的公共子表达式
 *   8. DCE — 清除死代码
 *   9. InstructionCombining — 合并剩余指令
 *   10. CSE — 再一轮 CSE（合并 IC 产生的冗余）
 *   11. DCE — 再一轮 DCE
 *
 * 默认顺序的缺陷：
 *   - StrengthReduction 在所有优化最后，错失降低计算强度的机会
 *   - 只做一轮 CSE+DCE，无法捕获后续 Pass 产生的新优化机会
 *   - LoopUnrolling 展开后的代码没有再次 CSE 处理
 */
int process_row(int row){
    int i = 0;
    int result = 0;
    while (i < N){
        /* 需要 StrengthReduction 做强度削减：i*5 改为 i<<2 + i */
        int idx1 = i * 5;       
        int idx2 = i * 10;      
        /* 需要 AlgebraicIdentities：i*1 = i, i+0 = i */
        int tmp1 = i * 1;       
        int tmp2 = i + 0;       
        /* 公共子表达式：两次相同的数组访问 */
        int v1 = a[row][i];
        int v2 = a[row][i];     
        int v3 = a[row][i];     
        /* 代数恒等式：v1*8 = v1<<3 */
        int v4 = v1 * 8;        
        int v5 = v2 * 4;        
        /* 循环不变量：N 不变，b[row][idx2] 可外提 */
        result = result + v1 + v4 + b[row][idx1] * 2 + b[row][idx2] / 2;
        i = i + 1;
    }
    return result;
}

int main(){
    int n = getint();
    int i = 0;
    
    /* 初始化 */
    while (i < N){
        int j = 0;
        while (j < N){
            a[i][j] = (i + j) & 31;
            b[i][j] = (i * j) & 63;
            j = j + 1;
        }
        i = i + 1;
    }
    
    starttime();
    i = 0;
    while (i < n){
        int row_idx = i % N;
        c[row_idx] = process_row(row_idx);
        i = i + 1;
    }
    stoptime();
    
    int ans = 0;
    i = 0;
    while (i < N){
        ans = ans + c[i];
        i = i + 1;
    }
    putint(ans);
    putch(10);
    return 0;
}
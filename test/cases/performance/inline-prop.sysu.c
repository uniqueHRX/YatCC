#include <sysy/sylib.h>
#define N 5000

int a[N], b[N], c[N], d[N], e[N], f[N];
int sum = 0;

/*
 * 挑战：多个辅助函数被循环调用（内联机会）
 * 常量通过函数参数/返回值传播，需要 CP 跨函数跟踪
 * 内联后产生冗余 store→load→store，需要 DSE+DCE 清除
 * 
 * 最佳顺序：
 *   1. Inliner — 将所有辅助函数内联到 loop_body 中
 *   2. Mem2Reg — 消除内联产生的 alloca/load/store 模式
 *   3. ConstantPropagation — 跟踪常量（如 BASE=1000, SCALE=2）
 *   4. ConstantFolding — 将常量表达式折叠
 *   5. LICM — 将循环不变的计算外提
 *   6. CSE — 消除重复子表达式
 *   7. DSE — 清除死store（内联+Mem2Reg后有很多）
 *   8. DCE — 清除死代码
 *   9. InstructionCombining — 合并剩余指令
 *
 * 默认顺序（Inliner→Mem2Reg→LICM→Unroll→CP→CF→CSE→DSE→DCE）的缺陷：
 *   LICM 在 CP/CF 之前，无法识别经过常量折叠才成为 invariant 的表达式
 *   如 SCALE*BASE 和 N+OFFSET 在 CF 之前不是常量形式
 */
int multiply(int x, int y){
    return x * y;
}

int add(int x, int y){
    return x + y;
}

int compute_val(int idx, int base, int scale, int offset){
    /* 编译时可知：base=1000, scale=2, offset=100 */
    /* 但需要在 Inliner+CP+CF 后才能折叠 */
    int t1 = multiply(base, scale);   /* = 2000 */
    int t2 = add(idx, offset);         /* = idx+100 */
    int t3 = add(t1, t2);              /* = 2000+idx+100 = idx+2100 */
    return t3;
}

int loop_body(int pos){
    /* 内联 compute_val 后：pos+2100 */
    int val = compute_val(pos, 1000, 2, 100);
    /* 冗余写入：连续两次写入相同位置 */
    a[pos] = val;
    a[pos] = val;       /* DSE 可消除 */
    
    b[pos] = val + 1;
    b[pos] = val + 1;   /* DSE 可消除 */
    
    c[pos] = val;
    d[pos] = val + val; /* CSE: val+val = 2*val */
    e[pos] = val * 2;   /* CSE: val*2 == val+val */
    
    /* 循环不变量：val 的计算在循环内每次都一样，可 LICM 外提 */
    /* 但需 CP/CF 先行才能识别为 invariant（经过函数内联和常量折叠后） */
    return val;
}

int main(){
    int n = getint();
    int i = 0;
    
    /* 初始化 */
    while (i < N){
        a[i] = 0; b[i] = 0; c[i] = 0;
        d[i] = 0; e[i] = 0; f[i] = 0;
        i = i + 1;
    }
    
    starttime();
    i = 0;
    while (i < n){
        int j = 0;
        while (j < N){
            int val = loop_body(j);
            /* val 的赋值在循环内，外层 sum 累加 */
            sum = sum + val;
            /* 死代码：下面这行赋值从未被读取 */
            f[j] = val * 2;
            j = j + 1;
        }
        i = i + 1;
    }
    stoptime();
    
    putint(sum);
    putch(10);
    return 0;
}
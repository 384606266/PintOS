#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

typedef int fixed_point;

#define SHIFT_NUM 16
/*convert value to fixed_point*/
#define CONVERT_TO_FIX(A) ((fixed_point)(A << SHIFT_NUM))
/*convert fixed_point to float*/
#define CONVERT_TO_FLOAT(FP) (FP >> SHIFT_NUM)
#define CONVERT_TO_FLOAT_ROUND(FP) (FP>=0 ? (FP+(1<<(SHIFT_NUM-1)))>>SHIFT_NUM : ((FP-(1<<(SHIFT_NUM-1)))>>SHIFT_NUM))
/*add A and B*/
#define ADD_FP(A,B) (A+B)
/*Subtract B from A:*/
#define SUB_FP(A,B) (A-B)
/*add A and int B*/
#define ADD_FP_INT(A,B) (A+(B<<SHIFT_NUM))
/*Subtract int B from A:*/
#define SUB_FP_INT(A,B) (A-(B<<SHIFT_NUM))

#define MUL_FP(A,B) ((fixed_point)((((int64_t)A) * B) >> SHIFT_NUM))
#define MUL_FP_INT(A,B) (A*B)
#define DIV_FP(A,B) ((fixed_point)((((int64_t)A) << SHIFT_NUM) / B))
#define DIV_FP_INT(A,B) (A/B)

#endif

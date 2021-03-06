/* 
 * CS:APP Data Lab 
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than, or equal to, the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
//1
/* 
 * bitAnd - x&y using only ~ and | 
 *   Example: bitAnd(6, 5) = 4
 *   Legal ops: ~ |
 *   Max ops: 8
 *   Rating: 1
 */
int bitAnd(int x, int y) {
  return ~((~x)|(~y));  
}
/*
 * isTmax - returns 1 if x is the maximum, two's complement number,
 *     and 0 otherwise 
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 1
 */
int isTmax(int x) {
    return !(~(x + x + 1) | !(~x));;
}
//2
/* 
 * getByte - Extract byte n from word x
 *   Bytes numbered from 0 (LSB) to 3 (MSB)
 *   Examples: getByte(0x12345678,1) = 0x56
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int getByte(int x, int n) {
    return (x >> (n << 3)) & 255;;
}
/* 
 * sign - return 1 if positive, 0 if zero, and -1 if negative
 *  Examples: sign(130) = 1
 *            sign(-23) = -1
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 10
 *  Rating: 2
 */
int sign(int x) {
    int m = !(x >> 31);
    return (~x + x) + m + (m & !!x);
}
/* 
 * allEvenBits - return 1 if all even-numbered bits in word set to 1
 *   Examples allEvenBits(0xFFFFFFFE) = 0, allEvenBits(0x55555555) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allEvenBits(int x) {
    int m = 85 | (85 << 8) | (85 << 16) | (85 << 24);
    return !((x & m) ^ m);
}
//3
/* 
 * bitMask - Generate a mask consisting of all 1's 
 *   lowbit and highbit
 *   Examples: bitMask(5,3) = 0x38
 *   Assume 0 <= lowbit <= 31, and 0 <= highbit <= 31
 *   If lowbit > highbit, then mask should be all 0's
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int bitMask(int highbit, int lowbit) {
    int m = ~((highbit + (~lowbit + 1)) >> 31);
    return m & (((~0 << highbit) << 1) ^ (~0 << lowbit)) ;
}
/*
 * satMul2 - multiplies by 2, saturating to Tmin or Tmax if overflow
 *   Examples: satMul2(0x30000000) = 0x60000000
 *             satMul2(0x40000000) = 0x7FFFFFFF (saturate to TMax)
 *             satMul2(0x80000001) = 0x80000000 (saturate to TMin)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3
 */
int satMul2(int x) {
  int s, sign, x2, r;
  sign = (x  >> 31);// & (((!!(x ^ (1 << 31))) << 31) >> 31);
  x2 = x + x;
  s = (x2 >> 31) ^ sign;
  //printf("%d, %d, %d\n", sign, x2, s);
  r = (s & (~(1 << 31) ^ sign)) | (x2 & (~s));
  //printf("res = %d\n", r);
  return r;
}

/*
 * ezThreeFourths - multiplies by 3/4 rounding toward 0,
 *   Should exactly duplicate effect of C expression (x*3/4),
 *   including overflow behavior.
 *   Examples: ezThreeFourths(11) = 8
 *             ezThreeFourths(-9) = -6
 *             ezThreeFourths(1073741824) = -268435456 (overflow)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 3
 */
int ezThreeFourths(int x) {
    int x3, bias, sign, r;
    x3 = (x << 1) + x;
    bias = 3;
    //printf("%d, 0x%.2x\n", x3, x3);
    sign = x3 >> 31;
    r = (sign & ((x3 + bias) >> 2)) + (~sign & (x3 >> 2));   
    //printf("0x%.2x\n", r);
    return r;
}
//4
/* 
 * sm2tc - Convert from sign-magnitude to two's complement
 *   where the MSB is the sign bit
 *   Example: sm2tc(0x80000005) = -5.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 4
 */
int sm2tc(int x) {
   int sign = x >> 31;
  return (sign & ((~x + 1) + (1 << 31))) + (~sign & x);
}
/*
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5) = 2, bitCount(7) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating: 4
 */
int bitCount(int x) {
    int mask, mask1, mask2, tmp;
    mask = (17 << 8) | 17;
    mask = (mask << 16) | mask; // 00010001^4
    mask1 = (15 << 8) | 15;  // 00001111^2
    mask2 = ((15 << 4) | 15) & (~mask1);
    mask2 = (mask2 << 8) | mask2; // 11110000^2
    tmp = (x & mask) + (mask & (x >> 1)) + (mask & (x >> 2)) + (mask & (x >> 3));
    tmp = tmp + (tmp >> 16);
    tmp = (tmp & mask1) + (((tmp & mask2)) >> 4);
    tmp = tmp + (tmp >> 8);
    return (tmp & 63);
}
/*
 * bitReverse - Reverse bits in a 32-bit word
 *   Examples: bitReverse(0x80000002) = 0x40000001
 *             bitReverse(0x89ABCDEF) = 0xF7D3D591
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 45
 *   Rating: 4
 */
int bitReverse(int x) {
    int tmp, mask1, mask2, mask4, mask8, mask16, tmin;
    tmin = 1 << 31;
   // mask1 = (0x55 << 8) | 0x55;
   // mask1 = ~((mask1 << 16) | mask1);  // 01010101
   // mask2 = (0x33 << 8) | 0x33;  
   // mask2 = ~((mask2 << 16) | mask2);  // 00110011
   // mask4 = (0x0f << 8) | 0x0f;
   // mask4 = ~((mask4 << 16) | mask4);  // 00001111
   // mask8 = ~(0xff << 16) | 0xff;    // 0 1 0 1
    mask16 = ((0xff << 8) | 0xff);    // 0 1
    mask8 = (mask16 << 8) ^ mask16;
    mask4 = (mask8 << 4) ^ mask8;
    mask2 = (mask4 << 2) ^ mask4;
    mask1 = (mask2 << 1) ^ mask2;
    mask1 = ~mask1;
    mask2 = ~mask2;
    mask4 = ~mask4;
    mask8 = ~mask8;
    mask16 = ~mask16;
    // printf("0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x\n", mask1, mask2, mask4, mask8, mask16);
    tmp = ((x << 16) & mask16) | (((x & mask16) >> 16) & (~mask16));
    tmp = ((tmp << 8) & mask8) | (((tmp & mask8) >> 8) & (~(tmin >> 7)));
    tmp = ((tmp << 4) & mask4) | (((tmp & mask4) >> 4) & (~(tmin >> 3)));
    tmp = ((tmp << 2) & mask2) | (((tmp & mask2) >> 2) & (~(tmin >> 1)));
    tmp = ((tmp << 1) & mask1) | (((tmp & mask1) >> 1) & (~tmin));
    return tmp;

}
//float
/* 
 * float_abs - Return bit-level equivalent of absolute value of f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   When argument is NaN, return argument..
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 10
 *   Rating: 2
 */
unsigned float_abs(unsigned uf) {
    unsigned sign, abs;
    sign  = (0xff << 1) << 22;
    abs = uf & (~(1 << 31));
    if(abs > sign)  return uf;
    return abs;
}
/* 
 * float_f2i - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int float_f2i(unsigned uf) {
    int nan, e, exp, frac, s, bias;
    nan = 0x80000000u;
    s = uf & (1 << 31);
    exp = (uf >> 23) & 0xff;
    frac = uf & (~(((0xff << 1) + 1) << 23));
    bias = 127;
    e = exp - bias;
    //printf("0x%.2x, 0x%.2x, %d\n", exp, frac, e); 
    if(exp == 0xff || e >= 31){
        return nan;
    }else if(!exp || e < 0){
        return 0;
    }else{
        s = s >> 31;
        frac = frac | (1 << 23);
        //printf("0x%.2x\n", frac);
        if(e < 23) frac = (frac >> (23 - e));
        else frac = (frac << (e - 23));
        //printf("0x%.2x, %d\n", frac, s);
        return (-frac & s) + (frac & (~s));
    }
}

/* 
 * float_twice - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_twice(unsigned uf) {
    int s, exp, frac;
    s = (uf >> 31) << 31;
    exp = (uf >> 23) & 0xff;
    frac = uf & (~(((0xff << 1) + 1) << 23));
    if(exp == 255) return uf;
    else if(!exp){
       frac = frac << 1;
       exp = (frac & (1 << 31)) >> 31;
    }else{
       if(exp != 255) {
           exp = exp + 1;
           frac = (!!(exp ^ 255)) * frac;
        }
    }
    return s | (exp << 23) | frac;  
}


SLJIT_API_FUNC_ATTRIBUTE const char* sljit_get_platform_name(void)
{
#if (defined SLJIT_CONFIG_LOONG_64 && SLJIT_CONFIG_LOONG_64)
	return "LoongArch-64" SLJIT_CPUINFO;
#else /* !SLJIT_CONFIG_LOONG_64 */
	return "LoongArch-32" SLJIT_CPUINFO;
#endif /* SLJIT_CONFIG_LOONG_32 */
}

typedef sljit_u32 sljit_ins;

/* Length of an instruction word
   Both for LoongArch-32 and LoongArch-64 */
#define TMP_REG1	(SLJIT_NUMBER_OF_REGISTERS + 2)
#define TMP_REG2	(SLJIT_NUMBER_OF_REGISTERS + 3)
#define TMP_REG3	(SLJIT_NUMBER_OF_REGISTERS + 4)
#define TMP_ZERO	0

/* Flags are kept in volatile registers. */
#define EQUAL_FLAG	(SLJIT_NUMBER_OF_REGISTERS + 5)
#define RETURN_ADDR_REG	TMP_REG2
#define OTHER_FLAG	(SLJIT_NUMBER_OF_REGISTERS + 6)

#define TMP_FREG1	(SLJIT_NUMBER_OF_FLOAT_REGISTERS + 1)
#define TMP_FREG2	(SLJIT_NUMBER_OF_FLOAT_REGISTERS + 2)


/*
	All general-purpose register mapping

	0            always zero
	4-11         $a0-$a7
	16-20        $s2-$s6           
	13-17        $t2-$t6     
	22           $s9
	31-23        $s8-$s0
	3            $sp
	13           $t1
	1            $ra
	14           $t2
	12           $t0
	15           $t3
*/

/* |             A              |          T            |                S                 | sp t1  ra t2  t0  t3 */
static const sljit_u8 reg_map[SLJIT_NUMBER_OF_REGISTERS + 7] = {
	0, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19, 20, 22, 31, 30, 29, 28, 27, 26, 25, 24, 23, 3, 13, 1, 14, 12, 15
};

/*
	All float-point register mapping

	0            always zero
	0-7          $fa0-$fa7
	10-23        $ft2-$ft15
	31-24        $fs7-$fs0
	8            $ft0
	9            $ft1
*/

/* |           A           |                            T                             |            S             | TMP | */
static const sljit_u8 freg_map[SLJIT_NUMBER_OF_FLOAT_REGISTERS + 3] = {
	0, 0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 31, 29, 28, 27, 26, 25, 24, 8, 9,
};


#define ADD_D       0x00108000
#define ADDI_D      0x02c00000
#define AND		      0x00148000
#define ANDI	      0x02300000
#define PCADDI      0x18000000
#define BEQ		      0x58000000
#define BNE		      0x5c000000
#define BLT		      0x60000000
#define BGE		      0x64000000
#define BLTU	      0x68000000
#define BGEU	      0x6c000000
#define DIV_D		    0x00220000
#define DIV_DU	    0x00230000
#define SYSCALL     0x002b0000
#define FADD_S      0x01008000
#define FDIV_S      0x01068000
#define FCMP_CEQ_S  0x0c120000 // WARN : NOT SURE >> RV FEQ.S performs a quiet comparison ===> LA fcmp.ceq(compareQuietEqual)
#define FLD_D       0x2b800000		
// NOTE : FLT.S and FLE.S perform what the IEEE 754-2008 standard refers to as signaling comparisons:
// that is, they set the invalid operation exception flag if either input is NaN.
#define FCMP_SLE_S  0x0c138000 // WARN : NOT SURE >> RV FLE_S less equal ===> LA fcmp.sle(compareSignalingLessEqual)
#define FCMP_SLT_S  0x0c118000 // WARN : NOT SURE >> RV FLT_S less than  ===> LA fcmp.slt(compareSignalingLess)
//
#define FCVT_S_D    0x01191800
// NOTE : FCVT.S.W or FCVT.S.L converts a 32-bit or 64-bit signed integer, respectively, in integer register rs1 into a
// floating-point number in floating-point register rd
// #define FCVT_S_W    0x         // WARN : NOT SURE >> RV FCVT_S_W  ===> LA MOVGR2FR_W + FFINT_S_W
#define MOVGR2FR_W  0x0114a400
#define FFINT_S_W   0x011d1000
// #define FCVT_W_S    0x         // WARN : NOT SURE >> RV FCVT_W_S  ===> LA FTINTRZ_W_S + MOVFR2GR_S
#define FTINTRZ_W_S 0x011a8400
#define MOVFR2GR_S  0x0114b400
#define FMUL_S	    0x01048000
// NOTE : FMV.X.W moves the single-precision value in floating-point register rs1 represented in IEEE 754-
// 2008 encoding to the lower 32 bits of integer register rd. The bits are not modified in the transfer,
// and in particular, the payloads of non-canonical NaNs are preserved. For RV64, the higher 32 bits
// of the destination register are filled with copies of the floating-point number’s sign bit
// #define FMV_X_W	    0x         // WARN : NOT SURE >> RV FMV_X_W  ===> MOVFR2GR_S  defined at ⬆️  
// #define FMV_W_X	    0x         // WARN : NOT SURE >> RV FMV_W_X  ===> MOVGR2FR_W
#define FST_D       0x2bc00000
// NOTE : FSGNJ.S rx, ry, ry moves ry
// to rx (assembler pseudoinstruction FMV.S rx, ry); FSGNJN.S rx, ry, ry moves the negation of ry
// to rx (assembler pseudoinstruction FNEG.S rx, ry); and FSGNJX.S rx, ry, ry moves the absolute
// value of ry to rx (assembler pseudoinstruction FABS.S rx, ry).
//
// fmv.s rd, rs fsgnj.s rd, rs, rs Copy single-precision register
// fabs.s rd, rs fsgnjx.s rd, rs, rs Single-precision absolute value
// fneg.s rd, rs fsgnjn.s rd, rs, rs Single-precision negate
// fmv.d rd, rs fsgnj.d rd, rs, rs Copy double-precision register
// fabs.d rd, rs fsgnjx.d rd, rs, rs Double-precision absolute value
// fneg.d rd, rs fsgnjn.d rd, rs, rs Double-precision negate
// #define FSGNJ_S	   0x
// #define FSGNJN_S    0x
// #define FSGNJX_S    0x
#define FMOV_S      0x01149400
#define FNEG_S	    0x01141400
#define FABS_S	    0x01140400

#define FSUB_S	    0x01028000
#define FST_S       0x2b400000
#define B           0x50000000
#define JIRL        0x4c000000
#define LD_D        0x28c00000
#define LU12I_W     0x14000000
#define LD_W        0x28800000
#define MUL_D       0x001d8000
#define MULH_D      0x001e0000
#define MULH_DU	    0x001e8000
#define OR		      0x00150000
#define ORI		      0x03800000
#define MOD_D       0x00228000
#define MOD_DU      0x00238000
#define ST_D        0x29c00000
#define SLL_D       0x00188000
#define SLLI	      0x00410000
#define SLT		      0x00120000
#define SLTI	      0x02000000
#define SLTU	      0x00128000
#define SLTUI	      0x02400000
#define SRL_D		    0x00190000
#define SRLI_D      0x00450000
#define SRA_D       0x00198000
#define SRAI_D      0x00490000
#define SUB_D       0x00118000
#define SW		      0x29800000
#define XOR		      0x00158000
#define XORI	      0x03c00000



#define BRANCH_MAX	(0xfff)
#define BRANCH_MIN	(-0x1000)
#define JUMP_MAX	(0xfffff)
#define JUMP_MIN	(-0x100000)

#if (defined SLJIT_CONFIG_LOONG_64 && SLJIT_CONFIG_LOONG_64)
#define S32_MAX		(0x7ffff7ffl)
#define S32_MIN		(-0x80000000l)
#define S44_MAX		(0x7fffffff7ffl)
#define S52_MAX		(0x7ffffffffffffl)
#endif

static sljit_s32 push_inst(struct sljit_compiler *compiler, sljit_ins ins)
{
	sljit_ins *ptr = (sljit_ins*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
	return SLJIT_SUCCESS;
}

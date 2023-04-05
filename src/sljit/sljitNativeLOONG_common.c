
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

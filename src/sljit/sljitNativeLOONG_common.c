
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

#define RD(rd)     ((sljit_ins)reg_map[rd] << 0)
#define RJ(rj)     ((sljit_ins)reg_map[rj] << 5)
#define RK(rk)     ((sljit_ins)reg_map[rk] << 10)
#define FRD(rd)    ((sljit_ins)freg_map[rd] << 0)
#define FRJ(rj)    ((sljit_ins)freg_map[rj] << 5)
#define FRK(rk)    ((sljit_ins)freg_map[rk] << 10)


#define IMM_SI12(imm)       ((sljit_ins)(imm) << 10)
#define IMM_UI6(imm)        ((sljit_ins)(imm) << 10)
#define IMM_UI12(imm)       ((sljit_ins)(imm) << 10)
#define IMM_SI20(imm)       ((sljit_ins)(imm) <<  5)
#define IMM_OFFS16(imm)     ((sljit_ins)(imm) << 10)
#define IMM_CODE15(imm)     ((((sljit_ins)(imm) & 0xffff) << 10) | (sljit_ins)(imm) >> 16)
#define IMM_OFFS2PART(imm)  ((sljit_ins)(imm) << 20)

#define ADD_D       0x00108000
#define ADDI_D      0x02c00000 // imm : si12[21:10]
#define AND         0x00148000
#define ANDI        0x02300000 // imm : ui12[21:10]
#define PCADDI      0x18000000 // imm : si20[24: 5]
#define BEQ         0x58000000 // imm : offs15[25:10]
#define BNE         0x5c000000 // imm : offs15[25:10]
#define BLT         0x60000000 // imm : offs15[25:10]
#define BGE         0x64000000 // imm : offs15[25:10]
#define BLTU        0x68000000 // imm : offs15[25:10]
#define BGEU        0x6c000000 // imm : offs15[25:10]
#define DIV_D       0x00220000
#define DIV_DU      0x00230000
#define SYSCALL     0x002b0000 // imm : code15[14: 0]
#define FADD_S      0x01008000
#define FDIV_S      0x01068000
#define FCMP_CEQ_S  0x0c120000 // WARN : NOT SURE >> RV FEQ.S performs a quiet comparison ===> LA fcmp.ceq(compareQuietEqual)

#define FLD_D       0x2b800000 // imm : si12[21:10]	
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
#define FMUL_S      0x01048000
// NOTE : FMV.X.W moves the single-precision value in floating-point register rs1 represented in IEEE 754-
// 2008 encoding to the lower 32 bits of integer register rd. The bits are not modified in the transfer,
// and in particular, the payloads of non-canonical NaNs are preserved. For RV64, the higher 32 bits
// of the destination register are filled with copies of the floating-point number’s sign bit
// #define FMV_X_W      0x         // WARN : NOT SURE >> RV FMV_X_W  ===> MOVFR2GR_S  defined at ⬆️  
// #define FMV_W_X      0x         // WARN : NOT SURE >> RV FMV_W_X  ===> MOVGR2FR_W

#define FST_D       0x2bc00000 // imm : si12[21:10]	
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
#define FNEG_S      0x01141400
#define FABS_S      0x01140400

#define FSUB_S      0x01028000
#define FST_S       0x2b400000 // imm : si12[21:10]	
#define B           0x50000000 // imm : offs(15:0)[25:10] + offs(25:16)[0:9]
#define JIRL        0x4c000000 // imm : offs15[25:10]
#define LD_D        0x28c00000 // imm : si12[21:10]	
#define LU12I_W     0x14000000 // imm : si20[24: 5]
#define LD_W        0x28800000 // imm : si12[21:10]
#define MUL_D       0x001d8000
#define MULH_D      0x001e0000
#define MULH_DU     0x001e8000
#define OR          0x00150000
#define ORI         0x03800000 // imm : ui12[21:10]
#define MOD_D       0x00228000
#define MOD_DU      0x00238000
#define ST_D        0x29c00000 // imm : si12[21:10]
#define SLL_D       0x00188000
#define SLLI_D      0x00410000 // imm : ui6[15:10]
#define SLT         0x00120000
#define SLTI        0x02000000 // imm : si12[21:10]
#define SLTU        0x00128000
#define SLTUI       0x02400000 // imm : si12[21:10]
#define SRL_D       0x00190000
#define SRLI_D      0x00450000 // imm : ui6[15:10]
#define SRA_D       0x00198000
#define SRAI_D      0x00490000 // imm : ui6[15:10]
#define SUB_D       0x00118000
#define ST_W        0x29800000 // imm : si12[21:10]
#define XOR         0x00158000
#define XORI        0x03c00000 // imm : ui12[21:10]


// FIXME : 
#define BRANCH_MAX	(0xfff)
#define BRANCH_MIN	(-0x1000)
#define JUMP_MAX	(0xfffff)
#define JUMP_MIN	(-0x100000)

#if (defined SLJIT_CONFIG_LOONG_64 && SLJIT_CONFIG_LOONG_64)
#define S32_MAX   (0x7ffff7ffl)
#define S32_MIN   (-0x80000000l)
#define S44_MAX   (0x7fffffff7ffl)
#define S52_MAX   (0x7ffffffffffffl)
#endif

static sljit_s32 push_inst(struct sljit_compiler *compiler, sljit_ins ins)
{
	sljit_ins *ptr = (sljit_ins*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
	return SLJIT_SUCCESS;
}


// FIXME : we need support a bunch of imm type here
static sljit_s32 push_imm_s_inst(struct sljit_compiler *compiler, sljit_ins ins, sljit_sw imm)
{
	return push_inst(compiler, ins | IMM_S(imm));
}

static SLJIT_INLINE sljit_ins* detect_jump_type(struct sljit_jump *jump, sljit_ins *code, sljit_sw executable_offset)
{
	sljit_sw diff;
	sljit_uw target_addr;
	sljit_ins *inst;

	inst = (sljit_ins *)jump->addr;

	if (jump->flags & SLJIT_REWRITABLE_JUMP)
		goto exit;

	if (jump->flags & JUMP_ADDR)
		target_addr = jump->u.target;
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		target_addr = (sljit_uw)(code + jump->u.label->size) + (sljit_uw)executable_offset;
	}

	diff = (sljit_sw)target_addr - (sljit_sw)inst - executable_offset;

	if (jump->flags & IS_COND) {
		inst--; // move to point to new instruction
		diff += SSIZE_OF(ins);

		if (diff >= BRANCH_MIN && diff <= BRANCH_MAX) {
			jump->flags |= PATCH_B;
			inst[0] = (inst[0] & 0x1fff07f) ^ 0x1000; // get addr imm from inst
			jump->addr = (sljit_uw)inst;
			return inst;
		}

		inst++;
		diff -= SSIZE_OF(ins);
	}

	if (diff >= JUMP_MIN && diff <= JUMP_MAX) {
		if (jump->flags & IS_COND) {
			inst[-1] -= (sljit_ins)(5 * sizeof(sljit_ins)) << 7;
		}

		jump->flags |= PATCH_J;
		return inst;
	}

	if (diff >= S32_MIN && diff <= S32_MAX) {
		if (jump->flags & IS_COND)
			inst[-1] -= (sljit_ins)(4 * sizeof(sljit_ins)) << 7;

		jump->flags |= PATCH_REL32;
		inst[1] = inst[0];
		return inst + 1;
	}

	if (target_addr <= (sljit_uw)S32_MAX) {
		if (jump->flags & IS_COND)
			inst[-1] -= (sljit_ins)(4 * sizeof(sljit_ins)) << 7;

		jump->flags |= PATCH_ABS32;
		inst[1] = inst[0];
		return inst + 1;
	}

	if (target_addr <= S44_MAX) {
		if (jump->flags & IS_COND)
			inst[-1] -= (sljit_ins)(2 * sizeof(sljit_ins)) << 7;

		jump->flags |= PATCH_ABS44;
		inst[3] = inst[0];
		return inst + 3;
	}

	if (target_addr <= S52_MAX) {
		if (jump->flags & IS_COND)
			inst[-1] -= (sljit_ins)(1 * sizeof(sljit_ins)) << 7;

		jump->flags |= PATCH_ABS52;
		inst[4] = inst[0];
		return inst + 4;
	}

exit:
	inst[5] = inst[0];
	return inst + 5;
}

// PATCH_* -> bits mask
static SLJIT_INLINE sljit_sw put_label_get_length(struct sljit_put_label *put_label, sljit_uw max_label)
{
	if (max_label <= (sljit_uw)S32_MAX) {
		put_label->flags = PATCH_ABS32;
		return 1;
	}

	if (max_label <= S44_MAX) {
		put_label->flags = PATCH_ABS44;
		return 3;
	}

	if (max_label <= S52_MAX) {
		put_label->flags = PATCH_ABS52;
		return 4;
	}

	put_label->flags = 0;
	return 5;
}

static SLJIT_INLINE void load_addr_to_reg(void *dst, sljit_u32 reg)
{
	struct sljit_jump *jump = NULL;
	struct sljit_put_label *put_label;
	sljit_uw flags;
	sljit_ins *inst;
	sljit_sw high;
	sljit_uw addr;

	if (reg != 0) {
		jump = (struct sljit_jump*)dst;
		flags = jump->flags;
		inst = (sljit_ins*)jump->addr;
		addr = (flags & JUMP_LABEL) ? jump->u.label->addr : jump->u.target;
	} else {
		put_label = (struct sljit_put_label*)dst;
		flags = put_label->flags;
		inst = (sljit_ins*)put_label->addr;
		addr = put_label->label->addr;
		reg = *inst;
	}

	if ((addr & 0x800) != 0)
		addr += 0x1000;

	if (flags & PATCH_ABS32) {
		SLJIT_ASSERT(addr <= S32_MAX);
		inst[0] = LUI | RD(reg) | (sljit_ins)((sljit_sw)addr & ~0xfff);
	} else if (flags & PATCH_ABS44) {
		high = (sljit_sw)addr >> 12;
		SLJIT_ASSERT((sljit_uw)high <= 0x7fffffff);

		if (high > S32_MAX) {
			SLJIT_ASSERT((high & 0x800) != 0);
			inst[0] = LUI | RD(reg) | (sljit_ins)0x80000000u;
			inst[1] = XORI | RD(reg) | RS1(reg) | IMM_I(high);
		} else {
			if ((high & 0x800) != 0)
				high += 0x1000;

			inst[0] = LUI | RD(reg) | (sljit_ins)(high & ~0xfff);
			inst[1] = ADDI | RD(reg) | RS1(reg) | IMM_I(high);
		}

		inst[2] = SLLI | RD(reg) | RS1(reg) | IMM_I(12);
		inst += 2;
	} else {
		high = (sljit_sw)addr >> 32;
		if ((addr & 0x80000000l) != 0)
			high = ~high;
		if ((high & 0x800) != 0)
			high += 0x1000;
		 
		if (flags & PATCH_ABS52) {
			SLJIT_ASSERT(addr <= S52_MAX);
			inst[0] = LUI | RD(TMP_REG3) | (sljit_ins)(high << 12);
		} else {
			inst[0] = LUI | RD(TMP_REG3) | (sljit_ins)(high & ~0xfff);
			inst[1] = ADDI | RD(TMP_REG3) | RS1(TMP_REG3) | IMM_I(high);
			inst++;
		}
		inst[1] = LUI | RD(reg) | (sljit_ins)((sljit_sw)addr & ~0xfff);
		inst[2] = SLLI | RD(TMP_REG3) | RS1(TMP_REG3) | IMM_I((flags & PATCH_ABS52) ? 20 : 32);
		inst[3] = XOR | RD(reg) | RS1(reg) | RS2(TMP_REG3);
		inst += 3;
	}

	if (jump != NULL) {
		SLJIT_ASSERT((inst[1] & 0x707f) == JALR);
		inst[1] = (inst[1] & 0xfffff) | IMM_I(addr);
	} else
		inst[1] = ADDI | RD(reg) | RS1(reg) | IMM_I(addr);
}


SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_ins *code;
	sljit_ins *code_ptr;
	sljit_ins *buf_ptr;
	sljit_ins *buf_end;
	sljit_uw word_count;
	sljit_uw next_addr;
	sljit_sw executable_offset;
	sljit_uw addr;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;
	struct sljit_put_label *put_label;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_generate_code(compiler));
	reverse_buf(compiler);

	code = (sljit_ins*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_ins), compiler->exec_allocator_data);
	PTR_FAIL_WITH_EXEC_IF(code);
	buf = compiler->buf;

	code_ptr = code;
	word_count = 0;
	next_addr = 0;
	executable_offset = SLJIT_EXEC_OFFSET(code);

	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	put_label = compiler->put_labels;

	do {
		buf_ptr = (sljit_ins*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr = *buf_ptr++;
			if (next_addr == word_count) {
				SLJIT_ASSERT(!label || label->size >= word_count);
				SLJIT_ASSERT(!jump || jump->addr >= word_count);
				SLJIT_ASSERT(!const_ || const_->addr >= word_count);
				SLJIT_ASSERT(!put_label || put_label->addr >= word_count);

				/* These structures are ordered by their address. */
				if (label && label->size == word_count) {
					label->addr = (sljit_uw)SLJIT_ADD_EXEC_OFFSET(code_ptr, executable_offset);
					label->size = (sljit_uw)(code_ptr - code);
					label = label->next;
				}
				if (jump && jump->addr == word_count) {
					word_count += 5;
					jump->addr = (sljit_uw)code_ptr;
					code_ptr = detect_jump_type(jump, code, executable_offset);
					jump = jump->next;
				}
				if (const_ && const_->addr == word_count) {
					const_->addr = (sljit_uw)code_ptr;
					const_ = const_->next;
				}
				if (put_label && put_label->addr == word_count) {
					SLJIT_ASSERT(put_label->label);
					put_label->addr = (sljit_uw)code_ptr;
					code_ptr += put_label_get_length(put_label, (sljit_uw)(SLJIT_ADD_EXEC_OFFSET(code, executable_offset) + put_label->label->size));
					word_count += 5;
					put_label = put_label->next;
				}
				next_addr = compute_next_addr(label, jump, const_, put_label);
			}
			code_ptr++;
			word_count++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	if (label && label->size == word_count) {
		label->addr = (sljit_uw)code_ptr;
		label->size = (sljit_uw)(code_ptr - code);
		label = label->next;
	}

	SLJIT_ASSERT(!label);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(!const_);
	SLJIT_ASSERT(!put_label);
	SLJIT_ASSERT(code_ptr - code <= (sljit_sw)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		do {
			if (!(jump->flags & (PATCH_B | PATCH_J | PATCH_REL32))) {
				load_addr_to_reg(jump, TMP_REG1);
				break;
			}

			addr = (jump->flags & JUMP_LABEL) ? jump->u.label->addr : jump->u.target;
			buf_ptr = (sljit_ins *)jump->addr;
			addr -= (sljit_uw)SLJIT_ADD_EXEC_OFFSET(buf_ptr, executable_offset);

			if (jump->flags & PATCH_B) {
				SLJIT_ASSERT((sljit_sw)addr >= BRANCH_MIN && (sljit_sw)addr <= BRANCH_MAX);
				addr = ((addr & 0x800) >> 4) | ((addr & 0x1e) << 7) | ((addr & 0x7e0) << 20) | ((addr & 0x1000) << 19);
				buf_ptr[0] |= (sljit_ins)addr;
				break;
			}

			if (jump->flags & PATCH_REL32) {
				SLJIT_ASSERT((sljit_sw)addr >= S32_MIN && (sljit_sw)addr <= S32_MAX);

				if ((addr & 0x800) != 0)
					addr += 0x1000;

				buf_ptr[0] = AUIPC | RD(TMP_REG1) | (sljit_ins)((sljit_sw)addr & ~0xfff);
				SLJIT_ASSERT((buf_ptr[1] & 0x707f) == JALR);
				buf_ptr[1] |= IMM_I(addr);
				break;
			}

			SLJIT_ASSERT((sljit_sw)addr >= JUMP_MIN && (sljit_sw)addr <= JUMP_MAX);
			addr = (addr & 0xff000) | ((addr & 0x800) << 9) | ((addr & 0x7fe) << 20) | ((addr & 0x100000) << 11);
			buf_ptr[0] = JAL | RD((jump->flags & IS_CALL) ? RETURN_ADDR_REG : TMP_ZERO) | (sljit_ins)addr;
		} while (0);
		jump = jump->next;
	}

	put_label = compiler->put_labels;
	while (put_label) {
		load_addr_to_reg(put_label, 0);
		put_label = put_label->next;
	}

	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_offset = executable_offset;
	compiler->executable_size = (sljit_uw)(code_ptr - code) * sizeof(sljit_ins);

	code = (sljit_ins *)SLJIT_ADD_EXEC_OFFSET(code, executable_offset);
	code_ptr = (sljit_ins *)SLJIT_ADD_EXEC_OFFSET(code_ptr, executable_offset);

	SLJIT_CACHE_FLUSH(code, code_ptr);
	SLJIT_UPDATE_WX_FLAGS(code, code_ptr, 1);
	return code;
}

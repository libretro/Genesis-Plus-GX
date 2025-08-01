//#include <stdio.h>

static FILE *fp_trace_m68k;
static char map_m68k[0x100 * 0x10000];


static char Dbg_Str[64];
static char Dbg_EA_Str[32];
static char Dbg_Size_Str[3];
static char Dbg_Cond_Str[3];

static unsigned short (*Next_Word)();
static unsigned int (*Next_Long)();

int trace_start;


static char *Make_Dbg_EA_Str(int Size, int EA_Num, int Reg_Num)
{
	int i;
	Dbg_EA_Str[31] = 0;

	switch(EA_Num)
	{
		case 0:
			// 000 rrr  Dr
			sprintf(Dbg_EA_Str, "D%.1d%c", Reg_Num, 0);
			break;

		case 1:
			// 001 rrr  Ar
			sprintf(Dbg_EA_Str, "A%.1d%c", Reg_Num, 0);
			break;

		case 2:
			// 010 rrr  (Ar)
			sprintf(Dbg_EA_Str, "(A%.1d)%c", Reg_Num, 0);
			break;

		case 3:
			// 011 rrr  (Ar)+
			sprintf(Dbg_EA_Str, "(A%.1d)+%c", Reg_Num, 0);
			break;

		case 4:
			// 100 rrr  -(Ar)
			sprintf(Dbg_EA_Str, "-(A%.1d)%c", Reg_Num, 0);
			break;

		case 5:
			// 101 rrr  d16(Ar)     dddddddd dddddddd
			sprintf(Dbg_EA_Str, "$%.4X(A%.1d)%c", Next_Word(), Reg_Num, 0);
			break;

		case 6:
			// 110 rrr  d8(Ar,ix)   aiiizcc0 dddddddd
			i = Next_Word() & 0xFFFF;
			if (i & 0x8000)
				sprintf(Dbg_EA_Str, "$%.2X(A%.1d,A%.1d)%c", i & 0xFF, Reg_Num, (i >> 12) & 0x7, 0);
			else
  				sprintf(Dbg_EA_Str, "$%.2X(A%.1d,D%.1d)%c", i & 0xFF, Reg_Num, (i >> 12) & 0x7, 0);
			break;

		case 7:
			switch(Reg_Num)
			{
				case 0:
					// 111 000  addr16      dddddddd dddddddd
					sprintf(Dbg_EA_Str, "($%.4X)%c", Next_Word(), 0);
					break;

				case 1:
					// 111 001  addr32      dddddddd dddddddd ddddddddd dddddddd
					sprintf(Dbg_EA_Str, "($%.8X)%c", Next_Long(), 0);
					break;

				case 2:
					// 111 010  d16(PC)     dddddddd dddddddd
					sprintf(Dbg_EA_Str, "$%.4X(PC)%c", Next_Word(), 0);
					break;

				case 3:
					// 111 011  d8(PC,ix)   aiiiz000 dddddddd
					i = Next_Word() & 0xFFFF;
					if (i & 0x8000)
						sprintf(Dbg_EA_Str, "$%.2X(PC,A%.1d)%c", i & 0xFF, (i >> 12) & 0x7, 0);
					else
						sprintf(Dbg_EA_Str, "$%.2X(PC,D%.1d)%c", i & 0xFF, (i >> 12) & 0x7, 0);
					break;

				case 4:
					// 111 100  imm/implied
					switch(Size)
					{
						case 0:
							sprintf(Dbg_EA_Str, "#$%.2X%c", Next_Word() & 0xFF, 0);
							break;

						case 1:
							sprintf(Dbg_EA_Str, "#$%.4X%c", Next_Word(), 0);
							break;

						case 2:
							sprintf(Dbg_EA_Str, "#$%.8X%c", Next_Long(), 0);
							break;
					}
					break;
			}
			break;
		}

	return(Dbg_EA_Str);
}


static char *Make_Dbg_Size_Str(int Size)
{
	Dbg_Size_Str[2] = 0;
	sprintf(Dbg_Size_Str, ".?");

	switch(Size)
	{
		case 0:
			sprintf(Dbg_Size_Str, ".B");
			break;

		case 1:
			sprintf(Dbg_Size_Str, ".W");
			break;

		case 2:
			sprintf(Dbg_Size_Str, ".L");
			break;
	}

	return(Dbg_Size_Str);
}


static char *Make_Dbg_Size_Str_2(int Size)
{
	Dbg_Size_Str[2] = 0;
	sprintf(Dbg_Size_Str, ".?");

	switch(Size)
	{
		case 0:
			sprintf(Dbg_Size_Str, ".W");
			break;

		case 1:
			sprintf(Dbg_Size_Str, ".L");
			break;
	}

	return(Dbg_Size_Str);
}

static char *Make_Dbg_Cond_Str(int Cond)
{
	Dbg_Cond_Str[2] = 0;
	sprintf(Dbg_Cond_Str, "??");

	switch(Cond)
	{
		case 0:
			sprintf(Dbg_Cond_Str, "Tr");
			break;

		case 1:
			sprintf(Dbg_Cond_Str, "Fa");
			break;

		case 2:
			sprintf(Dbg_Cond_Str, "HI");
			break;

		case 3:
			sprintf(Dbg_Cond_Str, "LS");
			break;

		case 4:
			sprintf(Dbg_Cond_Str, "CC");
			break;

		case 5:
			sprintf(Dbg_Cond_Str, "CS");
			break;

		case 6:
			sprintf(Dbg_Cond_Str, "NE");
			break;

		case 7:
			sprintf(Dbg_Cond_Str, "EQ");
			break;

		case 8:
			sprintf(Dbg_Cond_Str, "VC");
			break;

		case 9:
			sprintf(Dbg_Cond_Str, "VS");
			break;

		case 10:
			sprintf(Dbg_Cond_Str, "PL");
			break;

		case 11:
			sprintf(Dbg_Cond_Str, "MI");
			break;

		case 12:
			sprintf(Dbg_Cond_Str, "GE");
			break;

		case 13:
			sprintf(Dbg_Cond_Str, "LT");
			break;

		case 14:
			sprintf(Dbg_Cond_Str, "GT");
			break;

		case 15:
			sprintf(Dbg_Cond_Str, "LE");
			break;
    }

	return(Dbg_Cond_Str);
}


static char *M68KDisasm(unsigned short (*NW)(), unsigned int (*NL)(), unsigned int hook_pc )
{
	int i;
	unsigned short OPC;
	static char Tmp_Str[64];

	Dbg_Str[63] = 0;
	Tmp_Str[63] = 0;

	Next_Word = NW;
	Next_Long = NL;

	OPC = Next_Word();

	sprintf(Dbg_Str, "Unknown Opcode%c", 0);

	switch(OPC >> 12)
	{
		case 0:

		if (OPC & 0x100)
		{
			if ((OPC & 0x038) == 0x8)
			{
				if (OPC & 0x080)
					//MOVEP.z Ds,d16(Ad)
					sprintf(Dbg_Str, "MOVEP%-3sD%.1d,#$%.4X(A%.1d)%c", Make_Dbg_Size_Str_2((OPC & 0x40) >> 6), (OPC & 0xE00) >> 9, Next_Word(), OPC & 0x7, 0);
				else
					//MOVEP.z d16(As),Dd
					sprintf(Dbg_Str, "MOVEP%-3s#$%.4X(A%.1d),D%.1d%c", Make_Dbg_Size_Str_2((OPC & 0x40) >> 6), Next_Word(), OPC & 0x7, (OPC & 0xE00) >> 9, 0);
			}
			else
			{
				switch((OPC >> 6) & 0x3)
				{
					case 0:
						//BTST  Ds,a
						sprintf(Dbg_Str, "BTST    D%.1d,%s%c", (OPC & 0xE00) >> 9, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 7), 0);
						break;

					case 1:
						//BCHG  Ds,a
						sprintf(Dbg_Str, "BCHG    D%.1d,%s%c", (OPC & 0xE00) >> 9, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 7), 0);
						break;

					case 2:
						//BCLR  Ds,a
						sprintf(Dbg_Str, "BCLR    D%.1d,%s%c", (OPC & 0xE00) >> 9, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 7), 0);
						break;

					case 3:
						//BSET  Ds,a
						sprintf(Dbg_Str, "BSET    D%.1d,%s%c", (OPC & 0xE00) >> 9, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 7), 0);
						break;
				}
			}
		}
		else
		{
			switch((OPC >> 6) & 0x3F)
			{
				case 0:
					//ORI.B  #k,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "ORI.B   #$%.2X,%s%c", i, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 1:
					//ORI.W  #k,a
					i = Next_Word() & 0xFFFF;
					sprintf(Dbg_Str, "ORI.W   #$%.4X,%s%c", i, Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 2:
					//ORI.L  #k,a
					i = Next_Long();
					sprintf(Dbg_Str, "ORI.L   #$%.8X,%s%c", i, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 8:
					//ANDI.B  #k,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "ANDI.B  #$%.2X,%s%c", i & 0xFF, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 9:
					//ANDI.W  #k,a
					i = Next_Word() & 0xFFFF;
					sprintf(Dbg_Str, "ANDI.W  #$%.4X,%s%c", i, Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 10:
					//ANDI.L  #k,a
					i = Next_Long();
					sprintf(Dbg_Str, "ANDI.L  #$%.8X,%s%c", i, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 16:
					//SUBI.B  #k,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "SUBI.B  #$%.2X,%s%c", i & 0xFF, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 17:
					//SUBI.W  #k,a
					i = Next_Word() & 0xFFFF;
					sprintf(Dbg_Str, "SUBI.W  #$%.4X,%s%c", i, Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 18:
					//SUBI.L  #k,a
					i = Next_Long();
					sprintf(Dbg_Str, "SUBI.L  #$%.8X,%s%c", i, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 24:
					//ADDI.B  #k,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "ADDI.B  #$%.2X,%s%c", i & 0xFF, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 25:
					//ADDI.W  #k,a
					i = Next_Word() & 0xFFFF;
					sprintf(Dbg_Str, "ADDI.W  #$%.4X,%s%c", i, Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 26:
					//ADDI.L  #k,a
					i = Next_Long();
					sprintf(Dbg_Str, "ADDI.L  #$%.8X,%s%c", i, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 32:
					//BTST  #n,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "BTST    #%d,%s%c", i, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 33:
					//BCHG  #n,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "BCHG    #%d,%s%c", i, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 34:
					//BCLR  #n,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "BCLR    #%d,%s%c", i, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 35:
					//BSET  #n,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "BSET    #%d,%s%c", i, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 40:
					//EORI.B  #k,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "EORI.B  #$%.2X,%s%c", i & 0xFF, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 41:
					//EORI.W  #k,a
					i = Next_Word() & 0xFFFF;
					sprintf(Dbg_Str, "EORI.W  #$%.4X,%s%c", i, Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 42:
					//EORI.L  #k,a
					i = Next_Long();
					sprintf(Dbg_Str, "EORI.L  #$%.8X,%s%c", i, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 48:
					//CMPI.B  #k,a
					i = Next_Word() & 0xFF;
					sprintf(Dbg_Str, "CMPI.B  #$%.2X,%s%c", i & 0xFF, Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 49:
					//CMPI.W  #k,a
					i = Next_Word() & 0xFFFF;
					sprintf(Dbg_Str, "CMPI.W  #$%.4X,%s%c", i, Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 50:
					//CMPI.L  #k,a
					i = Next_Long();
					sprintf(Dbg_Str, "CMPI.L  #$%.8X,%s%c", i, Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;
			}
		}
		break;

		case 1:
			//MOVE.b  as,ad
			sprintf(Tmp_Str, "%s%c", Make_Dbg_EA_Str(0, (OPC >> 3) & 0x7, OPC & 0x7), 0);
			sprintf(Dbg_Str, "MOVE.b  %s,%s%c", Tmp_Str, Make_Dbg_EA_Str(0, (OPC >> 6) & 0x7, (OPC >> 9) & 0x7), 0);
			break;

		case 2:
			//MOVE.l  as,ad
			sprintf(Tmp_Str, "%s%c", Make_Dbg_EA_Str(2, (OPC >> 3) & 0x7, OPC & 0x7), 0);
			sprintf(Dbg_Str, "MOVE.l  %s,%s%c", Tmp_Str, Make_Dbg_EA_Str(2, (OPC >> 6) & 0x7, (OPC >> 9) & 0x7), 0);
			break;

		case 3:
			//MOVE.w  as,ad
			sprintf(Tmp_Str, "%s%c", Make_Dbg_EA_Str(1, (OPC >> 3) & 0x7, OPC & 0x7), 0);
			sprintf(Dbg_Str, "MOVE.w  %s,%s%c", Tmp_Str, Make_Dbg_EA_Str(1, (OPC >> 6) & 0x7, (OPC >> 9) & 0x7), 0);
			break;

		case 4:
			//SPECIALS ...

			if (OPC & 0x100)
			{
				if (OPC & 0x40)
					//LEA  a,Ad
					sprintf(Dbg_Str, "LEA     %s,A%.1d%c", Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
				else
					//CHK.W  a,Dd
					sprintf(Dbg_Str, "CHK.W   %s,D%.1d%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			}
			else
			{
				switch((OPC >> 6) & 0x3F)
				{
					case 0:	case 1: case 2:
						//NEGX.z  a
						sprintf(Dbg_Str, "NEGX%-4s%s%c", Make_Dbg_Size_Str((OPC >> 6) & 3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 3:
						//MOVE  SR,a
						sprintf(Dbg_Str, "MOVE    SR,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 8: case 9: case 10:
						//CLR.z  a
						sprintf(Dbg_Str, "CLR%-5s%s%c", Make_Dbg_Size_Str((OPC >> 6) & 3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 16: case 17: case 18:
						//NEG.z  a
						sprintf(Dbg_Str, "NEG%-5s%s%c", Make_Dbg_Size_Str((OPC >> 6) & 3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 19:
						//MOVE  a,CCR
						sprintf(Dbg_Str, "MOVE    %s,CCR%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 24: case 25: case 26:
						//NOT.z  a
						sprintf(Dbg_Str, "NOT%-5s%s%c", Make_Dbg_Size_Str((OPC >> 6) & 3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 27:
						//MOVE  a,SR
						sprintf(Dbg_Str, "MOVE    %s,SR%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 32:
						//NBCD  a
						sprintf(Dbg_Str, "NBCD    %s%c", Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 33:

						if (OPC & 0x38)
							//PEA  a
							sprintf(Dbg_Str, "PEA     %s%c", Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						else
							//SWAP.w  Dd
							sprintf(Dbg_Str, "SWAP.w  D%d%c", OPC & 0x7, 0);

						break;

					case 34: case 35:

						if (OPC & 0x38)
						{
							int registers_a7_d0 = Next_Word();

							//MOVEM.z Reg-List,a
							sprintf(Dbg_Str, "MOVEM%-3s{d0-a7}[%02x %02x],%s%c", Make_Dbg_Size_Str_2((OPC >> 6) & 1),
								registers_a7_d0 >> 8, registers_a7_d0 & 0xff,
								Make_Dbg_EA_Str((OPC >> 6) & 1 + 1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
							//Next_Word();
						}
						else
							//EXT.z  Dd
							sprintf(Dbg_Str, "EXT%-5s%s%c", Make_Dbg_Size_Str_2((OPC >> 6) & 1), Make_Dbg_EA_Str((OPC >> 6) & 1 + 1, (OPC & 0x38) >> 3, OPC & 0x7), 0);

						break;

					case 40: case 41: case 42:
						//TST.z a
						sprintf(Dbg_Str, "TST%-5s%s%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), Make_Dbg_EA_Str((OPC >> 6) & 0x3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 43:
						//TAS.b a
						sprintf(Dbg_Str, "TAS.B %s%c", Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 48: case 49:
						//Bad Opcode
						sprintf(Dbg_Str, "Bad Opcode%c", 0);
						break;

					case 50: case 51: {
						int registers_d0_a7 = Next_Word();

						//MOVEM.z a,Reg-List
						sprintf(Dbg_Str, "MOVEM%-3s%s,{a7-d0}[%02x %02x]%c", Make_Dbg_Size_Str_2((OPC >> 6) & 1), Make_Dbg_EA_Str((OPC >> 6) & 1 + 1, (OPC & 0x38) >> 3, OPC & 0x7),
							registers_d0_a7 >> 8, registers_d0_a7 & 0xff,
							0);
						//Next_Word();
						}	break;

					case 57:

						switch((OPC >> 3) & 0x7)
						{
							case 0: case 1:
								//TRAP  #vector
								sprintf(Dbg_Str, "TRAP    #$%.1X%c", OPC & 0xF, 0);
								break;

							case 2:
								//LINK As,#k16
								sprintf(Dbg_Str, "LINK    A%.1d,#$%.4X%c", OPC & 0x7, Next_Word(), 0);
								break;

							case 3:
								//ULNK Ad
								sprintf(Dbg_Str, "ULNK    A%.1d%c", OPC & 0x7, 0);
								break;

							case 4:
								//MOVE As,USP
								sprintf(Dbg_Str, "MOVE    A%.1d,USP%c",OPC & 0x7, 0);
								break;

							case 5:
								//MOVE USP,Ad
								sprintf(Dbg_Str, "MOVE    USP,A%.1d%c",OPC & 0x7, 0);
								break;

							case 6:

								switch(OPC & 0x7)
								{
									case 0:
										//RESET
										sprintf(Dbg_Str, "RESET%c", 0);
										break;

									case 1:
										//NOP
										sprintf(Dbg_Str, "NOP%c", 0);
										break;

									case 2:
										//STOP #k16
										sprintf(Dbg_Str, "STOP    #$%.4X%c", Next_Word(), 0);
										break;

									case 3:
										//RTE
										sprintf(Dbg_Str, "RTE%c", 0);
										break;

									case 4:
										//Bad Opcode
										sprintf(Dbg_Str, "Bad Opcode%c", 0);
										break;

									case 5:
										//RTS
										sprintf(Dbg_Str, "RTS%c", 0);
										break;

									case 6:
										//TRAPV
										sprintf(Dbg_Str, "TRAPV%c", 0);
										break;

									case 7:
										//RTR
										sprintf(Dbg_Str, "RTR%c", 0);
										break;
								}
								break;
						}
						break;

					case 58:
						//JSR  a
						sprintf(Dbg_Str, "JSR     %s%c", Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 59:
						//JMP  a
						sprintf(Dbg_Str, "JMP     %s%c", Make_Dbg_EA_Str(2, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;
				}
			}
			break;

		case 5:

			if ((OPC & 0xC0) == 0xC0)
			{
        char offset = OPC & 0xFF;
				
				if ((OPC & 0x38) == 0x08) {
					unsigned short word = Next_Word();

					//DBCC  Ds,label
					sprintf(Dbg_Str, "DB%-6sD%.1d,#$%.4X [%02X:%04X]%c", Make_Dbg_Cond_Str((OPC >> 8) & 0xF), OPC & 0x7, word,
            (hook_pc + 2) >> 16, (hook_pc + 2 + word) & 0xffff, 0);
				}
				else
					//STCC.b  a
					sprintf(Dbg_Str, "ST%-6s%s [%02X:%04X]%c", Make_Dbg_Cond_Str((OPC >> 8) & 0xF), Make_Dbg_EA_Str(0, (OPC & 0x38) >> 3, OPC & 0x7),
            (hook_pc + 2) >> 16, (hook_pc + 2 + offset) & 0xffff, 0);
				break;
			}
			else
			{
				if (OPC & 0x100)
					//SUBQ.z  #k3,a
					sprintf(Dbg_Str, "SUBQ%-4s#%.1d,%s%c", Make_Dbg_Size_Str((OPC >> 6) & 3), (OPC >> 9) & 0x7, Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
				else
					//ADDQ.z  #k3,a
					sprintf(Dbg_Str, "ADDQ%-4s#%.1d,%s%c", Make_Dbg_Size_Str((OPC >> 6) & 3), (OPC >> 9) & 0x7, Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
				break;
			}
			break;

		case 6:

			if (OPC & 0xFF)
			{
				int offset = (char) (OPC & 0xFF);

				if ((OPC & 0xF00) == 0x100)
				{
					//BSR  label
					sprintf(Dbg_Str, "BSR     #$%.2X [%02X:%04X]%c", OPC & 0xFF,
						(hook_pc + 2 + offset) >> 16, (hook_pc + 2 + offset) & 0xffff, 0);
					break;
				}

				if (!(OPC & 0xF00))
				{
					//BRA  label
					sprintf(Dbg_Str, "BRA     #$%.2X [%02X:%04X]%c", OPC & 0xFF,
						(hook_pc + 2 + offset) >> 16, (hook_pc + 2 + offset) & 0xffff, 0);
					break;
				}

				//BCC  label
				sprintf(Dbg_Str, "B%-7s#$%.2X [%02X:%04X]%c",
					Make_Dbg_Cond_Str((OPC >> 8) & 0xF), OPC & 0xFF,
					(hook_pc + 2 + offset) >> 16, (hook_pc + 2 + offset) & 0xffff, 0);
			}
			else
			{
				int offset = (short)(Next_Word());

				if ((OPC & 0xF00) == 0x100)
				{
					//BSR  label
					sprintf(Dbg_Str, "BSR     #$%.4X [%02X:%04X]%c", offset,
						(hook_pc + 2 + offset) >> 16, (hook_pc + 2 + offset) & 0xffff, 0);
					break;
				}

				if (!(OPC & 0xF00))
				{
					//BRA  label
					sprintf(Dbg_Str, "BRA     #$%.4X [%02X:%04X]%c", offset,
						(hook_pc + 2 + offset) >> 16, (hook_pc + 2 + offset) & 0xffff, 0);
					break;
				}

				//BCC  label
				sprintf(Dbg_Str, "B%-7s#$%.4X [%02X:%04X]%c",
					Make_Dbg_Cond_Str((OPC >> 8 ) & 0xF), offset,
					(hook_pc + 2 + offset) >> 16, (hook_pc + 2 + offset) & 0xffff, 0);
			}
			break;

		case 7:
			//MOVEQ  #k8,Dd
			sprintf(Dbg_Str, "MOVEQ   #$%.2X,D%.1d%c", OPC & 0xFF, (OPC >> 9) & 0x7, 0);
			break;

		case 8:

			if (OPC & 0x100)
  			{
				if (!(OPC & 0xF8))
				{
					//SBCD  Ds,Dd
					sprintf(Dbg_Str, "SBCD D%.1d,D%.1d%c", OPC & 0x7, (OPC >> 9) & 0x7, 0);
					break;
				}

				if ((OPC & 0xF8) == 0x8)
				{
					//SBCD  -(As),-(Ad)
					sprintf(Dbg_Str, "SBCD -(A%.1d),-(A%.1d)%c", OPC & 0x7, (OPC >> 9) & 0x7, 0);
					break;
				}

				if ((OPC & 0xC0) == 0xC0)
					//DIVS.w  a,Dd
					sprintf(Dbg_Str, "DIVS.W  %s,D%.1d%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
				else
					//OR.z  Ds,a
					sprintf(Dbg_Str, "OR%-6sD%.1d;%s%c", Make_Dbg_Size_Str((OPC >> 6) & 3), (OPC >> 9) & 0x7, Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
			}
			else
			{
				if ((OPC & 0xC0) == 0xC0)
					//DIVU.w  a,Dd
					sprintf(Dbg_Str, "DIVU.W  %s,D%.1d%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
				else
					//OR.z  a,Dd
					sprintf(Dbg_Str, "OR%-6s%s,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			}
			break;

		case 9:

			if ((OPC & 0xC0) == 0xC0)
				//SUBA.z  a,Ad
				sprintf(Dbg_Str, "SUBA%-4s%s,A%.1d%c", Make_Dbg_Size_Str_2((OPC >> 8) & 1), Make_Dbg_EA_Str((OPC >> 8) & 1 + 1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			else
			{
				if (OPC & 0x100)
				{
					if (!(OPC & 0x38))
					{
						//SUBX.z  Ds,Dd
						sprintf(Dbg_Str, "SUBX%-4sD%.1d,D%.1d%c",	Make_Dbg_Size_Str((OPC >> 6) & 0x3), OPC & 0x7, (OPC >> 9) & 0x7, 0);
						break;
					}

					if ((OPC & 0x38) == 0x8)
					{
						//SUBX.z  -(As),-(Ad)
						sprintf(Dbg_Str, "SUBX%-4s-(A%.1d),-(A%.1d)%c",	Make_Dbg_Size_Str((OPC >> 6) & 0x3), OPC & 0x7, (OPC >> 9) & 0x7, 0);
						break;
					}

					//SUB.z  Ds,a
					sprintf(Dbg_Str, "SUB%-5sD%.1d,%s%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
				}
				else
					//SUB.z  a,Dd
					sprintf(Dbg_Str, "SUB%-5s%s,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			}
			break;

		case 10:
			//Bad Opcode
			sprintf(Dbg_Str, "Bad Opcode%c", 0);
			break;

		case 11:

			if ((OPC & 0xC0) == 0xC0)
				//CMPA.z  a,Ad
				sprintf(Dbg_Str, "CMPA%-4s%s,A%.1d%c", Make_Dbg_Size_Str_2((OPC >> 8) & 1), Make_Dbg_EA_Str((OPC >> 7) & 1 + 1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			else
			{
				if (OPC & 0x100)
				{
					if ((OPC & 0x38) == 0x8)
					{
						//CMPM.z  (As)+,(Ad)+
						sprintf(Dbg_Str, "CMPM%-4s(A%.1d)+,(A%.1d)+%c",	Make_Dbg_Size_Str((OPC >> 6) & 0x3), OPC & 0x7, (OPC >> 9) & 0x7, 0);
						break;
					}

					//EOR.z  Ds,a
					sprintf(Dbg_Str, "EOR%-5sD%.1d,%s%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
				}
				else
					//CMP.z  a,Dd
					sprintf(Dbg_Str, "CMP%-5s%s,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			}
			break;

		case 12:

			if ((OPC & 0X1F8) == 0x100)
			{
				//ABCD Ds,Dd
				sprintf(Dbg_Str, "ABCD    D%.1d,D%.1d%c", OPC & 0x7, (OPC >> 9) & 0x7, 0);
				break;
			}

			if ((OPC & 0X1F8) == 0x140)
			{
				//EXG.l Ds,Dd
				sprintf(Dbg_Str, "EXG.L   D%.1d,D%.1d%c", OPC & 0x7, (OPC >> 9) & 0x7, 0);
				break;
			}

			if ((OPC & 0X1F8) == 0x108)
			{
				//ABCD -(As),-(Ad)
				sprintf(Dbg_Str, "ABCD    -(A%.1d),-(A%.1d)%c", OPC & 0x7, (OPC >> 9) & 0x7, 0);
				break;
			}

			if ((OPC & 0X1F8) == 0x148)
			{
				//EXG.l As,Ad
				sprintf(Dbg_Str, "EXG.L   A%.1d,A%.1d%c", OPC & 0x7, (OPC >> 9) & 0x7, 0);
				break;
			}

			if ((OPC & 0X1F8) == 0x188)
			{
				//EXG.l As,Dd
				sprintf(Dbg_Str, "EXG.L   A%.1d,D%.1d%c", OPC & 0x7, (OPC >> 9) & 0x7, 0);
				break;
			}

			switch((OPC	>> 6) & 0x7)
			{
				case 0: case 1: case 2:
					//AND.z  a,Dd
					sprintf(Dbg_Str, "AND%-5s%s,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
					break;

				case 3:
					//MULU.w  a,Dd
					sprintf(Dbg_Str, "MULU.W  %s,D%.1d%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
					break;

				case 4: case 5: case 6:
					//AND.z  Ds,a
					sprintf(Dbg_Str, "AND%-5sD%.1d,%s%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
					break;

				case 7:
					//MULS.w  a,Dd
					sprintf(Dbg_Str, "MULS.W  %s,D%.1d%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
					break;
			}
			break;

		case 13:

			if ((OPC & 0xC0) == 0xC0)
				//ADDA.z  a,Ad
				sprintf(Dbg_Str, "ADDA%-4s%s,A%.1d%c", Make_Dbg_Size_Str_2((OPC >> 8) & 1), Make_Dbg_EA_Str((OPC >> 8) & 1 + 1, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			else
			{
				if (OPC & 0x100)
				{
					if (!(OPC & 0x38))
					{
						//ADDX.z  Ds,Dd
						sprintf(Dbg_Str, "ADDX%-4sD%.1d,D%.1d%c",	Make_Dbg_Size_Str((OPC >> 6) & 0x3), OPC & 0x7, (OPC >> 9) & 0x7, 0);
						break;
					}

					if ((OPC & 0x38) == 0x8)
					{
						//ADDX.z  -(As),-(Ad)
						sprintf(Dbg_Str, "ADDX%-4s-(A%.1d),-(A%.1d)%c",	Make_Dbg_Size_Str((OPC >> 6) & 0x3), OPC & 0x7, (OPC >> 9) & 0x7, 0);
						break;
					}

					//ADD.z  Ds,a
					sprintf(Dbg_Str, "ADD%-5sD%.1d,%s%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), 0);
				}
				else
					//ADD.z  a,Dd
					sprintf(Dbg_Str, "ADD%-5s%s,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), Make_Dbg_EA_Str((OPC >> 6) & 3, (OPC & 0x38) >> 3, OPC & 0x7), (OPC >> 9) & 0x7, 0);
			}
			break;

		case 14:

			if ((OPC & 0xC0) == 0xC0)
			{
				switch ((OPC >> 8) & 0x7)
				{
					case 0:
						//ASR.w  #1,a
						sprintf(Dbg_Str, "ASR.W   #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;
	
					case 1:
						//ASL.w  #1,a
						sprintf(Dbg_Str, "ASL.W   #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 2:
						//LSR.w  #1,a
						sprintf(Dbg_Str, "LSR.W   #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 3:
						//LSL.w  #1,a
						sprintf(Dbg_Str, "LSL.W   #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 4:
						//ROXR.w  #1,a
						sprintf(Dbg_Str, "ROXR.W  #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 5:
						//ROXL.w  #1,a
						sprintf(Dbg_Str, "ROXL.W  #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 6:
						//ROR.w  #1,a
						sprintf(Dbg_Str, "ROR.W   #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;

					case 7:
						//ROL.w  #1,a
						sprintf(Dbg_Str, "ROL.W   #1,%s%c", Make_Dbg_EA_Str(1, (OPC & 0x38) >> 3, OPC & 0x7), 0);
						break;
 
				}
			}
			else
			{
				switch ((OPC >> 3) & 0x3F)
				{
					case 0: case 8: case 16:
						//ASR.z  #k,Dd
						sprintf(Dbg_Str, "ASR%-5s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 1: case 9: case 17:
						//LSR.z  #k,Dd
						sprintf(Dbg_Str, "LSR%-5s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 2: case 10: case 18:
						//ROXR.z  #k,Dd
						sprintf(Dbg_Str, "ROXR%-4s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 3: case 11: case 19:
						//ROR.z  #k,Dd
						sprintf(Dbg_Str, "ROR%-5s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 4: case 12: case 20:
						//ASR.z  Ds,Dd
						sprintf(Dbg_Str, "ASR%-5sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 5: case 13: case 21:
						//LSR.z  Ds,Dd
						sprintf(Dbg_Str, "LSR%-5sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 6: case 14: case 22:
						//ROXR.z  Ds,Dd
						sprintf(Dbg_Str, "ROXR%-4sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 7: case 15: case 23:
						//ROR.z  Ds,Dd
						sprintf(Dbg_Str, "ROR%-5sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 32: case 40: case 48:
						//ASL.z  #k,Dd
						sprintf(Dbg_Str, "ASL%-5s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 33: case 41: case 49:
						//LSL.z  #k,Dd
						sprintf(Dbg_Str, "LSL%-5s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 34: case 42: case 50:
						//ROXL.z  #k,Dd
						sprintf(Dbg_Str, "ROXL%-4s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 35: case 43: case 51:
						//ROL.z  #k,Dd
						sprintf(Dbg_Str, "ROL%-5s#%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 36: case 44: case 52:
						//ASL.z  Ds,Dd
						sprintf(Dbg_Str, "ASL%-5sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 37: case 45: case 53:
						//LSL.z  Ds,Dd
						sprintf(Dbg_Str, "LSL%-5sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 38: case 46: case 54:
						//ROXL.z  Ds,Dd
						sprintf(Dbg_Str, "ROXL%-4sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

					case 39: case 47: case 55:
						//ROL.z  Ds,Dd
						sprintf(Dbg_Str, "ROL%-5sD%.1d,D%.1d%c", Make_Dbg_Size_Str((OPC >> 6) & 0x3), (OPC >> 9) & 0x7, OPC & 0x7, 0);
						break;

				}
			}
			break;

		case 15:
			//Bad Opcode
			sprintf(Dbg_Str, "Bad Opcode%c", 0);
			break;
	}
	
	return(Dbg_Str);
}


static unsigned short Next_Word_T(void)
{
	return m68ki_read_imm_16();
}

static unsigned int Next_Long_T(void)
{
	return m68ki_read_imm_32();
}

static void trace_m68k()
{
	static char String [512];
	static unsigned int counter = 0;
	int real_pc = m68k.pc;


	if( trace_start == 0 ) return;


	counter++;

	if( map_m68k[m68k.pc] ) return;
	if( m68k.pc == 0x1F00 ) return;  // paprium diasm crash
	//if( m68k.pc >= 0xFF0000 ) return;
	//if( m68k.pc < 0x10000 ) return;
	map_m68k[m68k.pc] = 1;

	//if(m68k.pc == 0x84bf6 ) map_m68k[m68k.pc] = 0;


	if( !fp_trace_m68k ) {
		fp_trace_m68k = fopen("trace-m68k.txt", "w");
	}


	sprintf( String, "[%X] %02X:%04X", counter, m68k.pc >> 16, m68k.pc & 0xffff );
	fprintf( fp_trace_m68k, "%s", String );


	sprintf( String, "  %04X %04X  ", m68k_read_immediate_16(m68k.pc), m68k_read_immediate_16(m68k.pc+2) );
	fprintf( fp_trace_m68k, "%s", String );


	sprintf( String, "%-33s", M68KDisasm( Next_Word_T, Next_Long_T, m68k.pc ) );
	fprintf( fp_trace_m68k, "%s", String );


	sprintf( String, "A0=%.8X A1=%.8X A2=%.8X ", m68k.dar[8], m68k.dar[9], m68k.dar[10]);
	fprintf( fp_trace_m68k, "%s", String );

	sprintf( String, "A3=%.8X A4=%.8X A5=%.8X ", m68k.dar[11], m68k.dar[12], m68k.dar[13]);
	fprintf( fp_trace_m68k, "%s", String );

	sprintf( String, "A6=%.8X A7=%.8X", m68k.dar[14], m68k.dar[15]);
	fprintf( fp_trace_m68k, "%s", String );

	sprintf( String, "  " );
	fprintf( fp_trace_m68k, "%s", String );

	sprintf( String, "D0=%.8X D1=%.8X D2=%.8X ", m68k.dar[0], m68k.dar[1], m68k.dar[2]);
	fprintf( fp_trace_m68k, "%s", String );

	sprintf( String, "D3=%.8X D4=%.8X D5=%.8X ", m68k.dar[3], m68k.dar[4], m68k.dar[5]);
	fprintf( fp_trace_m68k, "%s", String );

	sprintf( String, "D6=%.8X D7=%.8X", m68k.dar[6], m68k.dar[7]);
	fprintf( fp_trace_m68k, "%s", String );

	fprintf( fp_trace_m68k, "  ");
	fprintf( fp_trace_m68k, "%c", (m68k.ir & 0x10)?'X':'x' );
	fprintf( fp_trace_m68k, "%c", (m68k.ir & 0x08)?'N':'n' );
	fprintf( fp_trace_m68k, "%c", (m68k.ir & 0x04)?'Z':'z' );
	fprintf( fp_trace_m68k, "%c", (m68k.ir & 0x02)?'V':'v' );
	fprintf( fp_trace_m68k, "%c", (m68k.ir & 0x01)?'C':'c' );

	fprintf( fp_trace_m68k, "\n" );
	fflush(fp_trace_m68k);


#if 0
	if( real_pc == 0xb90c2 )
		MessageBoxA(0,"debug","0",0);
#endif

	m68k.pc = real_pc;
}

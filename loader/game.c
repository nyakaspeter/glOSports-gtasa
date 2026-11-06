/* game.c -- Grand Theft Auto: San Andreas 2.00 (libGTASA.so): hooks and
 * fixes. The offsets (so_base + 0x...) are those of version 2.00 and come
 * from gtasa_vita (Andy Nguyen, Rinnegatamante, psykana, XirXes; MIT), with
 * its defaults: flying-vehicle camera on the right stick, no mobile widgets
 * and popups, cut radio tracks allowed, button actions on press, Resume loads
 * the newest save, Exit saves and quits. Its Vita-only rendering work
 * (Cg shader rewrites, PS2 colour filter, vitaGL tuning) is left out: the
 * game's own GLSL ES shaders run on Mali-400 as on Android. */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "android.h"
#include "so_util.h"
#include "util.h"

const char *game_so_name = "libGTASA.so";
const char *const game_required_files[] = {
	"libGTASA.so", "data/script/mainV1.scm", "data/script/scriptv1.img", NULL,
};

const JavaMethod game_java_methods[] = {
	COMMON_JAVA_METHODS,
	{ NULL, NULL },
};

/* ---- flying vehicles: the right stick stays the camera (gtasa_vita) ---- */

static void *(*CPad__GetPad)(int pad);
typedef SOFTFP int (*GetCarGunUpDown_t)(void *pad, int r1, int r2, float r3, int r4);
static GetCarGunUpDown_t CPad__GetCarGunUpDown;
static int (*CPad__GetSteeringLeftRight)(void *pad);
static int (*CPad__GetTurretLeft)(void *pad);
static int (*CPad__GetTurretRight)(void *pad);
static float *CTimer__ms_fTimeStep;

#define HARRIER_NOZZLE_ROTATE_LIMIT 5000
#define HARRIER_NOZZLE_ROTATERATE 25.0f

/* called from the stubs below, which expect the float back in r0 */
SOFTFP float CPlane__ProcessControlInputs_Rudder(char *this, int pad)
{
	float val;
	uint16_t model = *(uint16_t *)(this + 0x26);
	if (model == 539) {
		val = (float)CPad__GetSteeringLeftRight(CPad__GetPad(pad)) / 128.0f - *(float *)(this + 0x99C);
	} else if (CPad__GetTurretLeft(CPad__GetPad(pad))) {
		val = -1.0f - *(float *)(this + 0x99C);
	} else if (CPad__GetTurretRight(CPad__GetPad(pad))) {
		val = 1.0f - *(float *)(this + 0x99C);
	} else {
		val = 0.0f - *(float *)(this + 0x99C);
	}
	*(float *)(this + 0x99C) += val * 0.2f * *CTimer__ms_fTimeStep;
	return *(float *)(this + 0x99C);
}

void CPlane__ProcessControlInputs_Harrier(char *this, int pad)
{
	uint16_t model = *(uint16_t *)(this + 0x26);
	if (model != 520)
		return;
	float y = (float)CPad__GetCarGunUpDown(CPad__GetPad(pad), 0, 0, 2500.0f, 0);
	if (y > 10.0f || y < -10.0f) {
		int16_t *nozzle = (int16_t *)(this + 0x880);
		*(int16_t *)(this + 0x882) = *nozzle;
		*nozzle += (int16_t)(y / 128.0f * HARRIER_NOZZLE_ROTATERATE * *CTimer__ms_fTimeStep);
		if (*nozzle < 0)
			*nozzle = 0;
		else if (*nozzle > HARRIER_NOZZLE_ROTATE_LIMIT)
			*nozzle = HARRIER_NOZZLE_ROTATE_LIMIT;
	}
}

int CCam__Process_FollowCar_SA_camSetArrPos(char *this)
{
	uint16_t model = *(uint16_t *)(this + 0x26);
	if (model == 520 && *(int16_t *)(this + 0x880) >= 3000)
		return 2; /* heli */
	return model == 539 ? 0 : 3; /* car or plane */
}

uint64_t CCam__Process_FollowCar_SA_yMovement(char *this, uint32_t x, uint32_t y)
{
	if (*(uint16_t *)(this + 0x26) == 520)
		y = 0;
	return (uint64_t)x | (uint64_t)y << 32;
}

/* return addresses inside the game, set in patch_game */
uintptr_t ret_rudder, ret_harrier, ret_camsetarrpos, ret_ymovement;

__attribute__((naked)) void CPlane__ProcessControlInputs_Rudder_stub(void)
{
	__asm__ volatile(
		"push {r0-r11}\n"
		"mov r0, r4\n"
		"mov r1, r8\n"
		"bl CPlane__ProcessControlInputs_Rudder\n"
		"vmov s0, r0\n"
		"pop {r0-r11}\n"
		"ldr r12, =ret_rudder\n"
		"ldr r12, [r12]\n"
		"bx r12\n"
		".ltorg\n");
}

__attribute__((naked)) void CPlane__ProcessControlInputs_Harrier_stub(void)
{
	__asm__ volatile(
		"push {r0-r11}\n"
		"mov r0, r4\n"
		"mov r1, r8\n"
		"bl CPlane__ProcessControlInputs_Harrier\n"
		"pop {r0-r11}\n"
		"ldr r12, =ret_harrier\n"
		"ldr r12, [r12]\n"
		"bx r12\n"
		".ltorg\n");
}

__attribute__((naked)) void CCam__Process_FollowCar_SA_camSetArrPos_stub(void)
{
	__asm__ volatile(
		"push {r0-r8, r10-r11}\n"
		"mov r0, r11\n"
		"bl CCam__Process_FollowCar_SA_camSetArrPos\n"
		"mov r9, r0\n"
		"pop {r0-r8, r10-r11}\n"
		"ldr r12, =ret_camsetarrpos\n"
		"ldr r12, [r12]\n"
		"bx r12\n"
		".ltorg\n");
}

__attribute__((naked)) void CCam__Process_FollowCar_SA_yMovement_stub(void)
{
	__asm__ volatile(
		"push {r0-r11}\n"
		"mov r0, r11\n"
		"vmov r1, s21\n"
		"vmov r2, s28\n"
		"bl CCam__Process_FollowCar_SA_yMovement\n"
		"vmov s28, r1\n"
		"vmov s21, r0\n"
		"pop {r0-r11}\n"
		"ldr r12, =ret_ymovement\n"
		"ldr r12, [r12]\n"
		"bx r12\n"
		".ltorg\n");
}

/* ---- buttons ---- */

enum { MAPPING_JUMP = 3, MAPPING_CROUCH = 4, MAPPING_ENTER_CAR = 5, MAPPING_CAMERA_CLOSER = 9, MAPPING_CAMERA_FARTHER = 10 };
static int (*CHID__IsJustPressed)(int mapping);
static int (*CHID__IsReleased)(int mapping);

/* jump, crouch, enter car and camera distance act on press, not release */
static int CHID__IsReleased_Hook(int mapping)
{
	switch (mapping) {
	case MAPPING_JUMP:
	case MAPPING_CROUCH:
	case MAPPING_ENTER_CAR:
	case MAPPING_CAMERA_CLOSER:
	case MAPPING_CAMERA_FARTHER:
		return CHID__IsJustPressed(mapping);
	}
	return CHID__IsReleased(mapping);
}

/* While locked on, the game moves the lock to the next target with
 * CPad::ShiftTargetLeft/RightJustDown, which only answer a swipe on the
 * touch target button; the Xbox 360 layout binds LT and RT to PREV_TARGET
 * and NEXT_TARGET, read nowhere else. They answer those, and a flick of
 * the right stick (past half, from inside half the frame before; frames
 * by CTimer::m_FrameCounter). +0x110 is the pad's disabled flag. */
enum { MAPPING_NEXT_TARGET = 0x50, MAPPING_PREV_TARGET = 0x51 };
static uint32_t *timer_frame_counter;

static int stick_flick(void)
{
	static uint32_t frame;
	static float last;
	static int flick;
	uint32_t f = *timer_frame_counter;
	if (f != frame) {
		float x = pad_axis(2);
		float prev = f == frame + 1 ? last : x;
		flick = x <= -0.5f && prev > -0.5f ? -1 : x >= 0.5f && prev < 0.5f ? 1 : 0;
		frame = f;
		last = x;
	}
	return flick;
}

static int ShiftTargetLeftJustDown(char *pad)
{
	if (*(uint16_t *)(pad + 0x110))
		return 0;
	return CHID__IsJustPressed(MAPPING_PREV_TARGET) || stick_flick() < 0;
}

static int ShiftTargetRightJustDown(char *pad)
{
	if (*(uint16_t *)(pad + 0x110))
		return 0;
	return CHID__IsJustPressed(MAPPING_NEXT_TARGET) || stick_flick() > 0;
}

/* ---- saves and exit ---- */

static int (*CGenericGameStorage__CheckSlotDataValid)(int slot, int deleteRwObjects);
static void (*C_PcSave__GenerateGameFilename)(void *this, int slot, char *filename);
static uint64_t (*OS_FileGetDate)(int area, const char *path);
static void *PcSaveHelper;
static int *lastSaveForResume;
static int (*SaveGameForPause)(int type, char *cmd);

/* Resume loads the newest of the ten slots */
static int MainMenuScreen__HasCPSave(void)
{
	if (*lastSaveForResume == -1) {
		uint64_t latest = 0;
		for (int i = 0; i < 10; i++) {
			char filename[256];
			C_PcSave__GenerateGameFilename(&PcSaveHelper, i, filename);
			uint64_t date = OS_FileGetDate(1, filename);
			if (latest < date) {
				latest = date;
				*lastSaveForResume = i;
			}
		}
	}
	return CGenericGameStorage__CheckSlotDataValid(*lastSaveForResume, 1);
}

static int MainMenuScreen__OnExit(void)
{
	SaveGameForPause(3, NULL);
	exit_game(0);
}

/* the cheat hash table as the PC game has it (gtasa_vita: "Fixed cheats hash
 * key table"); cheats are typed with a keyboard the console doesn't have, so
 * this only keeps the table right */
static const uint32_t CCheat__m_aCheatHashKeys[] = {
	0xDE4B237D, 0xB22A28D1, 0x5A783FAE, 0x5A1B5E9A, 0x00000000, 0x00000000, 0x00000000,
	0x7B64E263, 0x00000000, 0x00000000, 0xEECCEA2B, 0x42AF1E28, 0x555FC201, 0x2A845345,
	0xE1EF01EA, 0x771B83FC, 0x5BF12848, 0x44453A17, 0x00000000, 0xB69E8532, 0x8B828076,
	0xDD6ED9E9, 0xA290FD8C, 0x00000000, 0x43DB914E, 0xDBC0DD65, 0x00000000, 0xD08A30FE,
	0x37BF1B4E, 0xB5D40866, 0xE63B0D99, 0x675B8945, 0x4987D5EE, 0x2E8F84E8, 0x00000000,
	0x00000000, 0x0D5C6A4E, 0x00000000, 0x00000000, 0x66516EBC, 0x4B137E45, 0x00000000,
	0x00000000, 0x3A577325, 0xD4966D59, 0x00000000, 0x5FD1B49D, 0xA7613F99, 0x1792D871,
	0xCBC579DF, 0x4FEDCCFF, 0x44B34866, 0x2EF877DB, 0x2781E797, 0x2BC1A045, 0xB2AFE368,
	0x00000000, 0x00000000, 0x1A5526BC, 0xA48A770B, 0x00000000, 0x00000000, 0x00000000,
	0x7F80B950, 0x6C0FA650, 0xF46F2FA4, 0x70164385, 0x00000000, 0x885D0B50, 0x151BDCB3,
	0xADFA640A, 0xE57F96CE, 0x040CF761, 0xE1B33EB9, 0xFEDA77F7, 0x00000000, 0x00000000,
	0xF53EF5A5, 0xF2AA0C1D, 0xF36345A8, 0x00000000, 0xB7013B1B, 0x00000000, 0x31F0C3CC,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF01286E9, 0xA841CC0A,
	0x31EA09CF, 0xE958788A, 0x02C83A7C, 0xE49C3ED4, 0x171BA8CC, 0x86988DAE, 0x2BDD2FA1,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* gtasa_vita patches the two script files once (ITB and the gym treadmill) */
static void patch_script(const char *file, const uint8_t *orig, const uint8_t *patch, long off)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", fs_root, file);
	FILE *f = fopen(path, "r+b");
	if (!f)
		fatal_error("can't open %s", path);
	uint8_t buf[6];
	fseek(f, off, SEEK_SET);
	if (fread(buf, 1, 6, f) == 6 && memcmp(buf, orig, 6) == 0) {
		fseek(f, off, SEEK_SET);
		fwrite(patch, 1, 6, f);
	}
	fclose(f);
}

static void patch_scripts(void)
{
	static const uint8_t op_00E1[6] = { 0xE1, 0x00, 0x04, 0x00, 0x04, 0x10 };
	static const uint8_t op_80E1[6] = { 0xE1, 0x80, 0x04, 0x00, 0x04, 0x10 };
	static const uint8_t nop_0A90[6] = { 0x90, 0x0A, 0x04, 0x40, 0x00, 0x00 };
	static const uint8_t nop_8A90[6] = { 0x90, 0x8A, 0x04, 0x40, 0x00, 0x00 };
	patch_script("data/script/mainV1.scm", op_00E1, nop_0A90, 0x00066703);
	patch_script("data/script/mainV1.scm", op_00E1, nop_0A90, 0x0006675A);
	patch_script("data/script/scriptv1.img", op_00E1, nop_0A90, 0x0004C0FF);
	patch_script("data/script/scriptv1.img", op_80E1, nop_8A90, 0x0004C131);
}

void patch_game(void)
{
	*(uint8_t *)so_find_addr("UseCloudSaves") = 0;
	*(uint8_t *)so_find_addr("UseTouchSense") = 0;
	*(int *)so_find_addr("gNoDetailTextures") = 1;

	/* allow the radio tracks cut from the mobile release (if restored) */
	hook_sym("_Z14IsRemovedTracki", ret0);
	hook_off(0x003A152A + 1, so_base + 0x003A1602 + 1); /* QueueUpTracksForStation */
	hook_off(0x003A35F6 + 1, so_base + 0x003A369A + 1); /* ChooseMusicTrackIndex */
	hook_off(0x003A37C2 + 1, so_base + 0x003A385E + 1); /* ChooseIdentIndex */
	hook_off(0x003A3A1E + 1, so_base + 0x003A3AA2 + 1); /* ChooseAdvertIndex */
	hook_off(0x003A4374 + 1, so_base + 0x003A4416 + 1); /* ChooseTalkRadioShow */
	hook_off(0x003A44D6 + 1, so_base + 0x003A4562 + 1); /* ChooseDJBanterIndexFromList */

	{
		/* FindPlayerVehicle calls as movs r0, #0: the right stick stays the camera */
		static const uint32_t movs_r0_0 = 0xBF002000;
		so_patch(0x003C0866, &movs_r0_0, 4);
		so_patch(0x003C1518, &movs_r0_0, 4);
		so_patch(0x003C198A, &movs_r0_0, 4);
		so_patch(0x003FC462, &movs_r0_0, 4);
		so_patch(0x003FC754, &movs_r0_0, 4);
		ret_camsetarrpos = so_base + 0x003C033A + 1;
		ret_ymovement = so_base + 0x003C1308 + 1;
		hook_off(0x003C057C + 1, CCam__Process_FollowCar_SA_camSetArrPos_stub);
		hook_off(0x003C12F4 + 1, CCam__Process_FollowCar_SA_yMovement_stub);
		CPad__GetPad = (void *)so_find_addr("_ZN4CPad6GetPadEi");
		CPad__GetCarGunUpDown = (void *)so_find_addr("_ZN4CPad15GetCarGunUpDownEbP11CAutomobilefb");
		CPad__GetSteeringLeftRight = (void *)so_find_addr("_ZN4CPad20GetSteeringLeftRightEv");
		CPad__GetTurretLeft = (void *)so_find_addr("_ZN4CPad13GetTurretLeftEv");
		CPad__GetTurretRight = (void *)so_find_addr("_ZN4CPad14GetTurretRightEv");
		CTimer__ms_fTimeStep = (float *)so_find_addr("_ZN6CTimer12ms_fTimeStepE");
		ret_rudder = so_base + 0x0057611A + 1;
		ret_harrier = so_base + 0x005765F0 + 1;
		hook_off(0x005760BA + 1, CPlane__ProcessControlInputs_Rudder_stub);
		hook_off(0x00576432 + 1, CPlane__ProcessControlInputs_Harrier_stub);
	}

	{
		static const uint16_t nop16 = 0xbf00;
		static const uint32_t nop32 = 0xbf00bf00;
		so_patch(0x0043A7A0, &nop32, 4); /* cutscene skip button */
		so_patch(0x004627E6, &nop32, 4);
		hook_sym("_ZN25CWidgetButtonMissionStart6UpdateEv", ret0); /* side mission buttons */
		hook_sym("_ZN26CWidgetButtonMissionCancel6UpdateEv", ret0);
		so_patch(0x003F91B6, &nop16, 2); /* steering control popup */
		hook_sym("_Z12Menu_ShowNagv", ret0); /* app rating popup */
		so_patch(0x0029E4AE, &nop32, 4); /* touch items in the controls menu */
		so_patch(0x0029E4E6, &nop32, 4);
		so_patch(0x0029E50A, &nop32, 4);
		so_patch(0x0029E530, &nop32, 4);
	}

	hook_off(0x002AADE0 + 1, so_base + 0x002AAF9A + 1); /* no explored-zone map highlight (alpha-heavy) */
	hook_off(0x004C6D16 + 1, so_base + 0x004C6E28 + 1); /* free aiming */
	hook_off(0x003C73F8 + 1, so_base + 0x003C7424 + 1); /* target switching fired twice */
	hook_off(0x0057629C + 1, so_base + 0x005762BC + 1); /* no automatic landing gear */

	hook_sym("_Z13SaveTelemetryv", ret0);
	hook_sym("_Z13LoadTelemetryv", ret0);
	hook_sym("_Z11updateUsageb", ret0);
	hook_sym("_Z14AND_FileUpdated", ret0);
	hook_sym("_Z17AND_BillingUpdateb", ret0);

	/* NVThreadGetCurrentJNIEnv stays the game's: it keeps the env in a
	 * pthread key (glibc's here) that NVThreadSpawnJNIThread's threads read
	 * back; gtasa_vita hooks it because its keys are stubs */
	hook_sym("_Z15OS_ThreadLaunchPFjPvES_jPKci16OSThreadPriority", OS_ThreadLaunch);
	hook_sym("_Z17OS_ScreenGetWidthv", OS_ScreenGetWidth);
	hook_sym("_Z18OS_ScreenGetHeightv", OS_ScreenGetHeight);
	hook_sym("_Z20AND_SystemInitializev", ret0);
	hook_sym("_Z13ProcessEventsb", ProcessEvents);
	hook_sym("_ZN14CAdjustableHUD10SaveToDiskEv", ret0);
	hook_sym("_ZN15CTouchInterface27RepositionAdjustableWidgetsEv", ret0);

	memcpy((void *)so_find_addr("_ZN6CCheat16m_aCheatHashKeysE"), CCheat__m_aCheatHashKeys, sizeof(CCheat__m_aCheatHashKeys));

	CHID__IsJustPressed = (void *)so_find_addr("_ZN4CHID13IsJustPressedE10HIDMapping");
	CHID__IsReleased = (void *)so_find_addr("_ZN4CHID10IsReleasedE10HIDMapping");
	timer_frame_counter = (uint32_t *)so_find_addr("_ZN6CTimer14m_FrameCounterE");
	hook_sym("_ZN4CPad23ShiftTargetLeftJustDownEv", ShiftTargetLeftJustDown);
	hook_sym("_ZN4CPad24ShiftTargetRightJustDownEv", ShiftTargetRightJustDown);
	hook_off(0x0018DFC4, CHID__IsReleased_Hook);

	/* Frame Limiter: outside the menu DoGameState waits for 1000/maxFPS ms
	 * a frame, maxFPS 20 with the limiter on, 30 with it off; now 30 on,
	 * 60 off (above the panel's ~55 Hz, so the flip sets the pace). The
	 * menu keeps its 30 */
	static const uint16_t movs_30 = 0x201e, movs_20 = 0x2014, movs_60 = 0x203c;
	if (*(uint16_t *)(so_base + 0x5E4990) == movs_30 && *(uint16_t *)(so_base + 0x5E4996) == movs_20) {
		so_patch(0x5E4990, &movs_60, 2);
		so_patch(0x5E4996, &movs_30, 2);
	} else {
		log_printf("game: frame limiter patch not applied\n");
	}

	CGenericGameStorage__CheckSlotDataValid = (void *)so_find_addr("_ZN19CGenericGameStorage18CheckSlotDataValidEib");
	C_PcSave__GenerateGameFilename = (void *)so_find_addr("_ZN8C_PcSave20GenerateGameFilenameEiPc");
	OS_FileGetDate = (void *)so_find_addr("_Z14OS_FileGetDate14OSFileDataAreaPKc");
	PcSaveHelper = (void *)so_find_addr("PcSaveHelper");
	lastSaveForResume = (void *)so_find_addr("lastSaveForResume");
	hook_sym("_ZN14MainMenuScreen9HasCPSaveEv", MainMenuScreen__HasCPSave);
	SaveGameForPause = (void *)so_find_addr("_Z16SaveGameForPause10eSaveTypesPc");
	hook_sym("_ZN14MainMenuScreen6OnExitEv", MainMenuScreen__OnExit);

	patch_scripts();
}

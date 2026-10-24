/* EcuM_Cfg.h - ELC EcuM configuration (no RTE; compile-time constants). */
#ifndef ECUM_CFG_H
#define ECUM_CFG_H

/* MCU Standby ("SLEEP") entry after post-run.
 *
 * 0 (default): the ECU never sleeps; after post-run and persistence it stays awake with CAN and diagnostics alive.
 * 1: after the post-run time and a finished NvM write EcuM stops the OS tick, disables all interrupts and enters
 *    STM32 Standby. The only wake-up source is the WAKE line on PA0 (WKUP1, rising edge; hardware note in
 *    Tool/Documentation/HW/Rev A/Sensing.kicad_sch: "use EXTI rising edge"). A wake-up is a reset: SRAM, including the
 *    retained window, is lost and the outputs are high impedance while sleeping. CAN cannot wake this board
 *    (TJA1051 has no wake pin), so enable this only when the vehicle wakes the ECU through the WAKE line.
 *
 * Sleep is only entered when: WAKE line low, ComM not in FULL_COMMUNICATION, no load command, load output off,
 * default diagnostic session, CanTp idle and the post-run NvM write has finished. Not validated on hardware. */
#ifndef ECUM_STANDBY_ENABLED
#define ECUM_STANDBY_ENABLED 0u
#endif

#define ECUM_RUN_TIMEOUT_TICKS      200u   /* 5 ms ticks without wake-up condition before post-run (1 s) */
#define ECUM_POSTRUN_TIMEOUT_TICKS  200u   /* 5 ms ticks of post-run before persistence and sleep (1 s) */

/* EcuM_SWState values (also consumed by SMon and the FreeRTOS tick hook, which stops restarting the ADC at SLEEP). */
#define ECUM_STATE_RUN              1u
#define ECUM_STATE_POST_RUN         2u
#define ECUM_STATE_SLEEP            3u

/* EcuM_GetWakeupReason() */
#define ECUM_WKSOURCE_NONE          0u     /* power-on or reset */
#define ECUM_WKSOURCE_WKUP_PIN      1u     /* woke up from Standby through the WAKE line (PA0) */

#endif /* ECUM_CFG_H */

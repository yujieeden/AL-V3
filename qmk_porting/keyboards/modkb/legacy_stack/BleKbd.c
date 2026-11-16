/********************************** (C) COPYRIGHT *******************************
 * File Name          : BleKbd.c
 * Description        : BLE HID keyboard application layer for the CH583 17-key
 *                      keypad.
 *
 *                      Layout of the three layers:
 *                        BLE_HID/          - WCH CH583EVT BLE stack + HID GATT
 *                                            profiles (verbatim copy, do not edit)
 *                        src/BleKbd.c      - THIS FILE: app task, advertising
 *                                            data, pairing, mesh of the report
 *                                            path to Kbd.c
 *                        src/Kbd.c         - matrix scan / LEDs / WS2812, shared
 *                                            with the USB transport
 *
 *                      The shared 8-byte Kbd.c state is converted to BLE's
 *                      14-byte NKRO report, using the same runtime Vial map
 *                      as USB's 6KRO report.
 *******************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "CONFIG.h"
#include <string.h>
#include "HAL.h"
#include "hiddev.h"
#include "hidkbdservice.h"
#include "battservice.h"
#include "BleKbd.h"
#include "Kbd.h"

// hidkbd.h �� WCH HID �������̵� APP ͷ��ֻ���� TMOS �����¼�λ��
//   START_DEVICE_EVT(0x0001) / START_REPORT_EVT(0x0002) /
//   START_PARAM_UPDATE_EVT(0x0004) / START_PHY_UPDATE_EVT(0x0008)
// �⼸���걾���ڹٷ����̵� hidkbd.h �������������(Profile/hiddev.c Ҳ����ͬһ��ͷ)��
// �¼�λ�������Լ��õ� SYS_EVENT_MSG(0x8000) / BLE_KB_TASK_EVT(0x0100) ����ͻ��
#include "hidkbd.h"

// �����̵İ���ɨ����� TMR0 �� 1ms �ж��ϣ���˯��(�µ�/��ͣģʽ)�� TMR0 ��ͣ�ˣ�
// ����Ҳ�����Զ��ָ�ɨ�裬��������ֱ�Ӱ� HAL_SLEEP ������
// Ҫ��˯�ߵ��Ȱ�ɨ��ĳ� RTC ���ѡ����ڻ��Ѻ����³�ʼ��ɨ�衣
#if (defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
#error "HAL_SLEEP must stay FALSE: the TMR0 matrix scan does not survive sleep (see BLE_HID/HAL/include/config.h)"
#endif

/*********************************************************************
 * MACROS
 */
// BLE NKRO input report length (modifier byte + 98 key bits + padding)
#define HID_KEYBOARD_IN_RPT_LEN              KB_BLE_NKRO_REPORT_SIZE

// HID LED output report length
#define HID_LED_OUT_RPT_LEN                  1

// Ask the host for a fast connection interval shortly after connecting.
// Ask the host for the keyboard-friendly 15-30ms / zero-latency parameters
// shortly after connection, without relying on the initial link defaults.
#define START_PARAM_UPDATE_EVT_DELAY         MS1_TO_SYSTEM_TIME(1000)

// How often the keypad task runs (ms).  Keeps key latency low; the task
// itself only sends a report when something changed.
#define KB_BLE_TASK_PERIOD                   10

// HID idle timeout in msec; 0 would disable the timeout
#define DEFAULT_HID_IDLE_TIMEOUT             60000

// Connection interval (units of 1.25ms): 15ms .. 30ms
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL    12
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL    24

// Slave latency to use if parameter update request
#define DEFAULT_DESIRED_SLAVE_LATENCY        0

// Supervision timeout value (units of 10ms) = 2s, above the interval/latency minimum
#define DEFAULT_DESIRED_CONN_TIMEOUT         200

// Pairing: no passkey, just-works, but keep the bond so the host
// reconnects automatically
#define DEFAULT_PASSCODE                     0
#define DEFAULT_PAIRING_MODE                 GAPBOND_PAIRING_MODE_WAIT_FOR_REQ
#define DEFAULT_MITM_MODE                    FALSE
#define DEFAULT_BONDING_MODE                 TRUE
#define DEFAULT_IO_CAPABILITIES              GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT

// ��ʱ���Ͽ��أ��ϵ��Ȳ���оƬ�ﱣ������а���Ϣ��
// ���� 0x3d(MIC failure) ����������ž����ʱ���ĳ� 1 ��һ�Ρ��ϵ���һ�Σ�
// Ȼ�����Ļ� 0 ���ջ������̼�������ÿ���ϵ綼��󶨣�������������ԣ���
#define BLE_KB_ERASE_BONDS_ON_BOOT          0

// Battery level is critical when it is less than this % (�ٷ�����ͬ��)
#define DEFAULT_BATT_CRITICAL_LEVEL          6

// ---------------------------------------------------------------------------
// Connection stability: advertising watchdog + fast/slow reconnect advertising.
// BLE_HID/ is a verbatim copy of the WCH CH583EVT tree and is not modified,
// so everything below lives in the application layer.
// ---------------------------------------------------------------------------

// Advertising watchdog period (ms).  It re-enables advertising whenever the
// stack is neither connected nor advertising.
#define BLE_KB_ADV_WATCHDOG_PERIOD           1000

// Reconnect advertising interval (units of 0.625ms) while the fast window
// after a disconnect is open: 48 = 30ms.
#define BLE_KB_FAST_ADV_INTERVAL             48

// Advertising interval used for initial pairing and after the fast window:
// 160 = 100ms.
#define BLE_KB_SLOW_ADV_INTERVAL             160

// Length of the fast reconnect window, in watchdog ticks (1 tick = 1s)
#define BLE_KB_FAST_ADV_SECONDS              30

// Print a health line at most this often while not connected (ticks = s)
#define BLE_KB_STATS_PERIOD                  60

/*********************************************************************
 * LOCAL VARIABLES
 */
// Task ID
static uint8_t bleKbdTaskId = INVALID_TASK_ID;

// Connection handle of the connected host
static uint16_t bleKbdConnHandle = GAP_CONNHANDLE_INIT;

// Set while a host connection exists, used for the debug print only
static uint8_t bleKbdConnected = FALSE;

// Last known report-path state, kept for diagnostics only.  The real check
// is performed by HidDev_Report() against the active connection's CCCD.
static uint8_t bleKbdReady = FALSE;

// ������Ͽ��أ������Ѿ����ϡ�����������������ȥʱ�����ÿ���һ��ԭ��
//   û���� "notifications on"  �� ����û���� HID ֪ͨ(CCCD ûд�ɹ�)
//   ���� "report failed"       �� Э��ջ����(��������·��û����/���û��ɣ�
//                                �� GATT ������ʱ��)
// ������Ը� 0��ʡ���⼸�д�ӡ��
#define BLE_KB_SEND_DEBUG   1
static uint32_t bleKbdLastSendErrTick = 0;

// �� BLE_HID/Profile/hiddev.c ͳ�ƣ�ֻ�������� HID ����ķ��ʣ�
// ȫ 0 = �������Ϻ����û����/д HID ����(�豸ʶ������� GATT ��������)
extern uint16_t hidDevDbgReadCnt;
extern uint16_t hidDevDbgWriteCnt;
extern uint16_t hidDevDbgCccdCnt;

// ---------------------------------------------------------------------------
// Link stability bookkeeping (updated by bleKbdStateCB / bleKbdAdvWatchdog)
// ---------------------------------------------------------------------------
static uint8_t  bleKbdAdvFast = FALSE;           // reconnect advertising mode
static uint8_t  bleKbdAdvSlowPending = FALSE;    // waiting to apply slow adv
static uint16_t bleKbdAdvFastTicks = 0;          // ticks left in the fast window
static uint16_t bleKbdWatchdogTicks = 0;         // ticks since the last stats line
static uint16_t bleKbdAdvRestartCount = 0;       // advertising (re)starts
static uint16_t bleKbdConnectCount = 0;          // link established events
static uint16_t bleKbdTerminateCount = 0;        // link terminated events
static uint8_t  bleKbdLastTerminateReason = 0;   // last HCI disconnect reason

// Advertised name, also used as the GATT device name characteristic.
// NOTE: GAP_DEVICE_NAME_LEN is 21, and the advertising packet must stay
// within 31 bytes: 3 (flags) + 4 (appearance) + 2 + strlen(name).
#define BLE_DEVICE_NAME    "CH583 Keypad"
#define BLE_DEVICE_NAME_LEN 12   // strlen(BLE_DEVICE_NAME)

static const uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = BLE_DEVICE_NAME;

// Advertising data
static uint8_t advertData[] = {
    // flags
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    // appearance: HID keyboard
    0x03,
    GAP_ADTYPE_APPEARANCE,
    LO_UINT16(GAP_APPEARE_HID_KEYBOARD),
    HI_UINT16(GAP_APPEARE_HID_KEYBOARD),

    // HID over GATT ���� UUID(0x1812)������Ž����㲥����
    // ���� Windows/��׿������ջֻ�����㲥���ж��豸���ͣ�UUID ֻ������
    // scan response ��ʱ�������"��Գɹ��������� HID ������Ҳ������
    // ���뱨��"������(����һֱû�� notifications on)��
    0x03,
    GAP_ADTYPE_16BIT_MORE,
    LO_UINT16(HID_SERV_UUID),
    HI_UINT16(HID_SERV_UUID),

    // complete local name: 1(AD type) + 12(name) = 13 �ֽ�
    0x0D,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', '5', '8', '3', ' ', 'K', 'e', 'y', 'p', 'a', 'd',
};

// Scan response: preferred connection interval + HID service UUID
static uint8_t scanRspData[] = {
    0x05,
    GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
    HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),

    0x05,
    GAP_ADTYPE_16BIT_MORE,
    LO_UINT16(HID_SERV_UUID),
    HI_UINT16(HID_SERV_UUID),
    LO_UINT16(BATT_SERV_UUID),
    HI_UINT16(BATT_SERV_UUID),

    // Tx power level
    0x02,
    GAP_ADTYPE_POWER_LEVEL,
    0
};

// HID device configuration
static hidDevCfg_t bleKbdHidCfg = {
    DEFAULT_HID_IDLE_TIMEOUT, // idle timeout
    HID_FEATURE_FLAGS         // feature flags (remote wake)
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void    bleKbdProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static uint8_t bleKbdRcvReport(uint8_t len, uint8_t *pData);
static uint8_t bleKbdReportCB(uint8_t id, uint8_t type, uint16_t uuid,
                              uint8_t oper, uint16_t *pLen, uint8_t *pData);
static void    bleKbdEvtCB(uint8_t evt);
static void    bleKbdStateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);
static void    bleKbdAdvIntervalSet(void);
static void    bleKbdAdvStart(void);
static void    bleKbdAdvWatchdog(void);

// HID device profile callbacks.  The passcode callback stays NULL: the keypad
// is a just-works device with no display and no keypad to type a passcode on.
static hidDevCB_t bleKbdHidCBs = {
    bleKbdReportCB,
    bleKbdEvtCB,
    NULL,
    bleKbdStateCB};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */
void BleKbd_Init(void)
{
    KB_Ble_Connected = 1;   // ����ģʽ��������ͽ� WS2812 �ƴ���
                            // WS_Show ��� BLE/RTC �жϣ��ڹ㲥/����/����κν׶ζ��к�
    bleKbdTaskId = TMOS_ProcessEventRegister(BleKbd_ProcessEvent);

    // ----- ˳�����Ҫ���չٷ� HID_Keyboard ���̵� main() ����
    //   GAPRole_PeripheralInit()  �� GAP �ӻ���ɫ���� RF/���ӹ���������
    //   HidDev_Init()             ע�� GAP/GATT/DeviceInfo/Battery/ScanParam ����
    //                             �����Ϲ㲥״̬�ص�(HID �⿿��ά�� hidDevGapState)
    //   Hid_AddService()          ע�� HID ����(������ HidDev_Init ֮��
    //                             ��Ϊ��Ҫ�� Battery ����� handle)
    //   HidDev_Register()         ֻ������ȫ��ָ��(���� + �ص�)���������ǰ��֮��
    GAPRole_PeripheralInit();
    HidDev_Init();

#if BLE_KB_ERASE_BONDS_ON_BOOT
    // һ������� SNV ��ľɰ󶨣��������豸�� LTK ��һ��ʱ��������·��ֱ��
    // �� 0x3d(MIC failure) �Ͽ�����������־���"�����ϵ��򲻳���"��
    HidDev_SetParameter(HIDDEV_ERASE_ALLBONDS, 0, NULL);
    PRINT("ble: all bonds erased\n");
#endif

    // ----- GAP peripheral role: load the data first, then enable -----
    {
        uint8_t advEnable = TRUE;

        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
        bleKbdAdvIntervalSet();
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advEnable);
    }

    // ----- ���ƿɽ��ܵ����Ӽ�����ܾ� Windows �ĳ���ʡ���� -----
    // Windows ����ʱ��� HID ���̵����Ӽ������ 0xa0(200ms)/0x9c(195ms)��
    // ��ô���ļ�����Զ������¼��ʹ��� 0x08 �ල��ʱ���������豸�ɽ��ܵ�
    // ��������Ƶ� 0x18(30ms)�������;ܾ������Ӿ����ȶ����֡�
    {
        uint16_t minInt = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
        uint16_t maxInt = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
        GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t), &minInt);
        GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t), &maxInt);
    }

    // ----- PHY: ֻ�� 1M���������ļ�����ƣ�����������-----
    // 2M PHY ����Ƶ����Ҫ��ߣ�������������������/������Բ�͵���
    // (��־�� Phy update 2M �� 0x3d/0x08)���� 1M ���ȣ�1M ������������
    // ���ʡ�磬������� 30ms ���޾ܾ������Բ������� 180ms ��ʱ��
    {
        uint8_t phy = GAP_PHY_BIT_LE_1M;
        GAPRole_SetParameter(GAPROLE_PHY_TX_SUPPORTED, sizeof(uint8_t), &phy);
        GAPRole_SetParameter(GAPROLE_PHY_RX_SUPPORTED, sizeof(uint8_t), &phy);
    }

    // ----- Set 15-30ms interval, zero latency and 2s supervision timeout -----
    // �ӻ��ĳ�ʼ���Ӽ������������������� GAP �ཨ��ƫ��Ҳ���ͬһ��ֵ��
    // Timeout is 2000ms, strictly above 30ms * (0 + 1).
    GAP_SetParamValue(TGAP_CONN_EST_INT_MIN, DEFAULT_DESIRED_MIN_CONN_INTERVAL);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MAX, DEFAULT_DESIRED_MAX_CONN_INTERVAL);
    GAP_SetParamValue(TGAP_CONN_EST_LATENCY, DEFAULT_DESIRED_SLAVE_LATENCY);
    GAP_SetParamValue(TGAP_CONN_EST_SUPERV_TIMEOUT, DEFAULT_DESIRED_CONN_TIMEOUT);

    // ----- GAP device name + appearance -----
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (void *)attDeviceName);
    {
        // GAP Appearance ����Ҳ������ HID ����(0x03C1)����������������
        // ������������������ HID ����������Ļ�Ĭ���� 0(Unknown)��
        uint16_t appearance = GAP_APPEARE_HID_KEYBOARD;
        GGS_SetParameter(GGS_APPEARANCE_ATT, sizeof(uint16_t), &appearance);
    }

    // ----- GAP bond manager: just-works pairing, keep the bond -----
    {
        uint32_t passkey = DEFAULT_PASSCODE;
        uint8_t  pairMode = DEFAULT_PAIRING_MODE;
        uint8_t  mitm = DEFAULT_MITM_MODE;
        uint8_t  ioCap = DEFAULT_IO_CAPABILITIES;
        uint8_t  bonding = DEFAULT_BONDING_MODE;

        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE, sizeof(uint8_t), &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION, sizeof(uint8_t), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    }

    // Setup Battery Characteristic Values (�͹ٷ�����һ��)
    {
        uint8_t critical = DEFAULT_BATT_CRITICAL_LEVEL;
        Batt_SetParameter(BATT_PARAM_CRITICAL_LEVEL, sizeof(uint8_t), &critical);
    }

    // ----- HID keyboard service -----
    Hid_AddService();

    // Register for HID dev callback
    HidDev_Register(&bleKbdHidCfg, &bleKbdHidCBs);

    // �� �ؼ���ע���������ͺ����������������ģʽ�� KB_Send() �ķ��ͺ�������
    // NULL���������ĩβ UsbHid_Exit() ��������ˣ���������������������ȥ��
    KB_SetSendFunc(BleKbd_Notify);

    // Kick off the (delayed) profile startup
    tmos_set_event(bleKbdTaskId, START_DEVICE_EVT);

    // �ؼ������������Եİ�������(ÿ KB_BLE_TASK_PERIOD=10ms һ��)��
    // KB_Task() ����Ѿ���״̬��� HID ������֮ǰֻ�� BleKbd_Process() ��
    // TMOS_SystemProcess() ���غ���Ǵ�ֱ�ӵ��ã�TMOS ����ʱ���������¼�֮��
    // ����KB_Task() ����һ�����һ�Ρ������̰��� 1ms �ж��ﰴ����̧��
    // �� KB_Task() ����ʱ�Ѿ���"�ɿ�"״̬��������Զ������ȥ ���� ���־���
    // �����ӡ��ܶ��ġ�LED Ҳ���յ�����������ȫû��Ӧ��
    tmos_start_task(bleKbdTaskId, BLE_KB_TASK_EVT,
                    MS1_TO_SYSTEM_TIME(KB_BLE_TASK_PERIOD));

    // Advertising watchdog: keeps the keypad reconnectable even if a state
    // transition drops the one-shot advertising restart in bleKbdStateCB.
    tmos_start_task(bleKbdTaskId, BLE_KB_ADV_WATCHDOG_EVT,
                    MS1_TO_SYSTEM_TIME(BLE_KB_ADV_WATCHDOG_PERIOD));
}

/*********************************************************************
 * @fn      BleKbd_ProcessEvent
 *
 * @brief   BLE keyboard app task.  Started with a 10ms timer so key state
 *          changes reach the host quickly.
 *
 * @return  events not processed
 */
uint16_t BleKbd_ProcessEvent(uint8_t task_id, uint16_t events)
{
    (void)task_id;

    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;

        if((pMsg = tmos_msg_receive(bleKbdTaskId)) != NULL)
        {
            bleKbdProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            tmos_msg_deallocate(pMsg);      // release the TMOS message
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & START_DEVICE_EVT)
    {
        // Nothing else to do, the profile startup happens in HidDev_Init()
        return (events ^ START_DEVICE_EVT);
    }

    if(events & START_PARAM_UPDATE_EVT)
    {
        // Ask for a short connection interval so keys feel responsive
        GAPRole_PeripheralConnParamUpdateReq(bleKbdConnHandle,
                                             DEFAULT_DESIRED_MIN_CONN_INTERVAL,
                                             DEFAULT_DESIRED_MAX_CONN_INTERVAL,
                                             DEFAULT_DESIRED_SLAVE_LATENCY,
                                             DEFAULT_DESIRED_CONN_TIMEOUT,
                                             bleKbdTaskId);
        return (events ^ START_PARAM_UPDATE_EVT);
    }

    if(events & BLE_KB_ADV_WATCHDOG_EVT)
    {
        bleKbdAdvWatchdog();
        return (events ^ BLE_KB_ADV_WATCHDOG_EVT);
    }

    if(events & BLE_KB_TASK_EVT)
    {
        // Same job main() does for USB, just from the TMOS scheduler
        KB_Task();
        tmos_start_task(bleKbdTaskId, BLE_KB_TASK_EVT, MS1_TO_SYSTEM_TIME(KB_BLE_TASK_PERIOD));
        return (events ^ BLE_KB_TASK_EVT);
    }

    return 0;
}

void BleKbd_Process(void)
{
    // Runs the profile/HID/GATT tasks, then the keyboard report task
    TMOS_SystemProcess();
    KB_Task();
}

uint8_t BleKbd_Notify(uint8_t *report)
{
    uint8_t status;
    static uint8_t nkroReport[KB_BLE_NKRO_REPORT_SIZE];
    uint8_t i;

    if(!bleKbdConnected)
    {
        return 0;                   // no active link yet
    }

    if(report == KB_Report)
    {
        KB_NKRO_ReportBuild(nkroReport);
    }
    else
    {
        memset(nkroReport, 0, sizeof(nkroReport));
        nkroReport[0] = report[0];
        for(i = 2; i < KB_REPORT_SIZE; i++)
        {
            uint8_t usage = report[i];
            if((usage >= 0x04) && (usage <= 0x65))
            {
                uint8_t bit = (uint8_t)(usage - 0x04);
                nkroReport[1 + (bit >> 3)] |= (uint8_t)(1U << (bit & 0x07));
            }
        }
    }

    status = Hid_SetParameter(HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT,
                              REPORT_UUID, HID_KEYBOARD_IN_RPT_LEN, nkroReport);
    if(status != SUCCESS)
    {
        return 0;
    }

    status = HidDev_Report(HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT,
                           HID_KEYBOARD_IN_RPT_LEN, nkroReport);

    if(status == SUCCESS)
    {
        bleKbdReady = TRUE;
        return 1;
    }

#if BLE_KB_SEND_DEBUG
    if((uint32_t)(KB_Tick - bleKbdLastSendErrTick) >= 1000)
    {
        bleKbdLastSendErrTick = KB_Tick;
        if(status == blePending)
        {
            PRINT("ble: report pending, tx busy (status=0x%02x)\n", status);
        }
        else if((status == bleNoResources) || (status == bleNotReady))
        {
            PRINT("ble: report dropped, no HID notify (status=0x%02x gatt r=%d w=%d cccd=%d)\n",
                  status, hidDevDbgReadCnt, hidDevDbgWriteCnt, hidDevDbgCccdCnt);
        }
        else
        {
            PRINT("ble: report failed, status=0x%02x\n", status);
        }
    }
#endif

    // blePending / bleMemAllocError just means the controller is busy this
    // round; the caller retries with a short backoff, so nothing is lost.
    return 0;
}

uint8_t BleKbd_Connected(void)
{
    return bleKbdReady;
}

/*********************************************************************
 * @fn      bleKbdAdvIntervalSet
 *
 * @brief   Load TGAP_DISC_ADV_INT_MIN/MAX for the current fast/slow mode.
 *          The new interval takes effect the next time advertising is enabled.
 */
static void bleKbdAdvIntervalSet(void)
{
    uint16_t interval = bleKbdAdvFast ? BLE_KB_FAST_ADV_INTERVAL
                                      : BLE_KB_SLOW_ADV_INTERVAL;

    GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, interval);
    GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, interval);
}

/*********************************************************************
 * @fn      bleKbdAdvStart
 *
 * @brief   (Re)start advertising with the interval for the current mode.
 *          Used by the watchdog and by the disconnect path; a redundant call
 *          while already advertising is harmless.
 */
static void bleKbdAdvStart(void)
{
    uint8_t advEnable = TRUE;

    bleKbdAdvIntervalSet();
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advEnable);
    bleKbdAdvRestartCount++;
}

/*********************************************************************
 * @fn      bleKbdAdvWatchdog
 *
 * @brief   1s advertising watchdog.  Handles the case where the stack lands
 *          in WAITING / ERROR and the one-shot restart in bleKbdStateCB did
 *          not take effect, which would otherwise leave the keypad silent.
 *          Also downgrades the fast reconnect window to the slow interval.
 */
static void bleKbdAdvWatchdog(void)
{
    uint8_t state = GAPROLE_INIT;

    if(GAPRole_GetParameter(GAPROLE_STATE, &state) != SUCCESS)
    {
        state = GAPROLE_ERROR;
    }
    state &= GAPROLE_STATE_ADV_MASK;

    if((state == GAPROLE_CONNECTED) || (state == GAPROLE_CONNECTED_ADV))
    {
        // Link is up: no advertising work, and the fast window is over.
        bleKbdAdvFast = FALSE;
        bleKbdAdvSlowPending = FALSE;
        bleKbdAdvFastTicks = 0;
        bleKbdWatchdogTicks = 0;
    }
    else
    {
        // Expire the fast reconnect window.
        if(bleKbdAdvFast && (bleKbdAdvFastTicks > 0) && (--bleKbdAdvFastTicks == 0))
        {
            bleKbdAdvFast = FALSE;
            bleKbdAdvSlowPending = TRUE;
        }

        if(bleKbdAdvSlowPending)
        {
            if(state == GAPROLE_ADVERTISING)
            {
                // Stop the fast advertising; the next tick restarts it slow.
                uint8_t advEnable = FALSE;

                GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advEnable);
                tmos_start_task(bleKbdTaskId, BLE_KB_ADV_WATCHDOG_EVT,
                                MS1_TO_SYSTEM_TIME(BLE_KB_ADV_WATCHDOG_PERIOD));
                return;
            }

            bleKbdAdvSlowPending = FALSE;
            bleKbdAdvStart();
        }
        else if(state != GAPROLE_ADVERTISING)
        {
            bleKbdAdvStart();
        }

        if(++bleKbdWatchdogTicks >= BLE_KB_STATS_PERIOD)
        {
            bleKbdWatchdogTicks = 0;
            PRINT("ble: stats conn=%d term=%d last=0x%02x adv=%d state=%d\n",
                  (int)bleKbdConnectCount, (int)bleKbdTerminateCount,
                  (int)bleKbdLastTerminateReason, (int)bleKbdAdvRestartCount,
                  (int)state);
        }
    }

    tmos_start_task(bleKbdTaskId, BLE_KB_ADV_WATCHDOG_EVT,
                    MS1_TO_SYSTEM_TIME(BLE_KB_ADV_WATCHDOG_PERIOD));
}

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      bleKbdProcessTMOSMsg
 *
 * @brief   Process an incoming task message (none are used so far).
 */
static void bleKbdProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch(pMsg->event)
    {
        default:
            break;
    }
}

/*********************************************************************
 * @fn      bleKbdRcvReport
 *
 * @brief   Process an incoming HID output report (host -> keypad), i.e. the
 *          LED indicator byte (NumLock / CapsLock / ScrollLock).
 *
 * @return  SUCCESS / ATT error code
 */
static uint8_t bleKbdRcvReport(uint8_t len, uint8_t *pData)
{
    if(len == HID_LED_OUT_RPT_LEN)
    {
        // Same byte USB gets on EP1 OUT, drives the WS2812 NumLock colour
        KB_SetLEDState(pData[0]);
        PRINT("ble LED = 0x%02x\n", pData[0]);
        return SUCCESS;
    }

    return ATT_ERR_INVALID_VALUE_SIZE;
}

/*********************************************************************
 * @fn      bleKbdReportCB
 *
 * @brief   HID device report callback: LED report writes, report reads and
 *          CCCD (notification enable/disable) writes.
 */
static uint8_t bleKbdReportCB(uint8_t id, uint8_t type, uint16_t uuid,
                              uint8_t oper, uint16_t *pLen, uint8_t *pData)
{
    uint8_t status = SUCCESS;

    if(oper == HID_DEV_OPER_WRITE)
    {
        if(uuid == REPORT_UUID)
        {
            // only the LED output report is interesting, ignore the rest
            if(type == HID_REPORT_TYPE_OUTPUT)
            {
                status = bleKbdRcvReport(*pLen, pData);
            }
        }

        if(status == SUCCESS)
        {
            status = Hid_SetParameter(id, type, uuid, *pLen, pData);
        }
    }
    else if(oper == HID_DEV_OPER_READ)
    {
        status = Hid_GetParameter(id, type, uuid, pLen, pData);
        PRINT("ble: report read id=%d type=%d uuid=0x%04x len=%d\n",
              id, type, uuid, *pLen);
    }
    else if(oper == HID_DEV_OPER_ENABLE)
    {
        // Host subscribed to the keyboard input report
        bleKbdReady = bleKbdConnected ? TRUE : FALSE;
        PRINT("ble: notifications on\n");

        // �����Ѿ����ĳɹ���HID �������������ˣ���ʱ��������������Ӽ����
        // ����������/�����֡�
        tmos_start_task(bleKbdTaskId, START_PARAM_UPDATE_EVT,
                        START_PARAM_UPDATE_EVT_DELAY);
    }
    else if(oper == HID_DEV_OPER_DISABLE)
    {
        bleKbdReady = FALSE;
        PRINT("ble: notifications off\n");
    }

    return status;
}

/*********************************************************************
 * @fn      bleKbdEvtCB
 *
 * @brief   HID device events (suspend / exit suspend / boot / report mode).
 */
static void bleKbdEvtCB(uint8_t evt)
{
    // Nothing to do: the keypad has no low power mode and no boot mode
    // handling of its own, the report layout is identical in both.
    (void)evt;
}

/*********************************************************************
 * @fn      bleKbdStateCB
 *
 * @brief   GAP state change callback: drives the debug prints, the
 *          "can I send a report" flag and re-advertising.
 */
static void bleKbdStateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch(newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
        {
            uint8_t ownAddr[6];

            GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddr);
            GAP_ConfigDeviceAddr(ADDRTYPE_STATIC, ownAddr);
            PRINT("ble: initialized, addr %02x:%02x:%02x:%02x:%02x:%02x\n",
                  ownAddr[5], ownAddr[4], ownAddr[3], ownAddr[2], ownAddr[1], ownAddr[0]);
        }
        break;

        case GAPROLE_ADVERTISING:
            if(pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT)
            {
                PRINT("ble: advertising as \"%s\"\n", BLE_DEVICE_NAME);
            }
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;

                bleKbdConnHandle = event->connectionHandle;
                bleKbdConnected = TRUE;
                KB_Ble_Connected = 1;   // �����ڼ�ƴ����ֽ���
                bleKbdReady = FALSE;    // set once the host subscribes
                bleKbdConnectCount++;
                bleKbdAdvFast = FALSE;
                bleKbdAdvSlowPending = FALSE;
                bleKbdAdvFastTicks = 0;
                // ע�⣺���Ӳ������²��������﷢��ԭ���̶�"���Ӻ� 1s"�Ĳ�����
                // ��ʱ���������������/�����ֹ����У������������ HID ���ã�
                // ���������ղ�����֪ͨ���ĳɵ��������ĳɹ�(notifications on)
                // ֮�������󣬼� bleKbdReportCB �� ENABLE ��֧��
                PRINT("ble: connected (%d)\n", (int)bleKbdConnectCount);
            }
            break;

        case GAPROLE_WAITING:
            if(pEvent->gap.opcode == GAP_END_DISCOVERABLE_DONE_EVENT)
            {
                PRINT("ble: advertising ended\n");
            }
            else if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
            {
                bleKbdTerminateCount++;
                bleKbdLastTerminateReason = pEvent->linkTerminate.reason;
                PRINT("ble: disconnected, reason 0x%02x (term=%d adv=%d)\n",
                      pEvent->linkTerminate.reason, (int)bleKbdTerminateCount,
                      (int)bleKbdAdvRestartCount);
            }
            else if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                PRINT("ble: advertising timeout\n");
            }

            if(bleKbdConnected)
            {
                // Disconnected: forget the report/LED state so that a
                // reconnect does not inherit a stale "key still down" report
                bleKbdConnected = FALSE;
                bleKbdReady = FALSE;
                bleKbdConnHandle = GAP_CONNHANDLE_INIT;
                KB_ResetReport();
            }

            // Reconnect fast for a while when a host is already bonded, then
            // fall back to the slow interval.  The watchdog performs the
            // downgrade; here we only pick the mode and restart advertising.
            {
                uint8_t bondCount = 0;

                if(GAPBondMgr_GetParameter(GAPBOND_BOND_COUNT, &bondCount) != SUCCESS)
                {
                    bondCount = 0;
                }

                bleKbdAdvFast = (bondCount > 0) ? TRUE : FALSE;
                bleKbdAdvSlowPending = FALSE;
                bleKbdAdvFastTicks = bleKbdAdvFast ? BLE_KB_FAST_ADV_SECONDS : 0;
                bleKbdWatchdogTicks = 0;

                bleKbdAdvStart();
                PRINT("ble: advertising %s, bonds=%d\n",
                      bleKbdAdvFast ? "fast" : "slow", (int)bondCount);
            }
            break;

        case GAPROLE_ERROR:
            PRINT("ble: error, opcode 0x%02x\n", pEvent->gap.opcode);
            break;

        default:
            break;
    }
}

/******************************** endfile @ BleKbd ******************************/

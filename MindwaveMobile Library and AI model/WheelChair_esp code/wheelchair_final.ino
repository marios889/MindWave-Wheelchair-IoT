#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include "MPU6050_6Axis_MotionApps20.h"

// ===================== BTS7960 Motor 1 (RIGHT) =====================
#define EN1    33
#define LPWM1  25
#define RPWM1  26
// ===================== BTS7960 Motor 2 (LEFT) =====================
#define EN2    27
#define LPWM2  14
#define RPWM2  12

#define PWM_FREQ      20000
#define PWM_RES_BITS  8
#define PWM_MAX       255

// ===================== Ultrasonics =====================
const uint8_t TRIG_LEFT  = 19;
const uint8_t ECHO_LEFT  = 18;
const uint8_t TRIG_RIGHT = 5;
const uint8_t ECHO_RIGHT = 17;

const float OBSTACLE_STOP_CM   = 70.0f;  // trip distance
const float OBSTACLE_CLEAR_CM  = 75.0f;  // must exceed this to clear (hysteresis, avoids edge chatter)
unsigned long lastLeftPing = 0, lastRightPing = 0;
float leftDistCm = 999, rightDistCm = 999;
const unsigned long PING_INTERVAL_MS = 60;

// Small rolling sample buffer per sensor so a single dropped/garbage echo
// (pulseIn timeout or stray reflection) can't trip or clear the obstacle flag on its own.
const int US_HISTORY_SIZE = 3;
float leftDistHistory[US_HISTORY_SIZE]  = {999, 999, 999};
float rightDistHistory[US_HISTORY_SIZE] = {999, 999, 999};
int leftHistIdx = 0, rightHistIdx = 0;

float medianOf3(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }
  if (b > c) { float t = b; b = c; c = t; }
  if (a > b) { float t = a; a = b; b = t; }
  return b;
}

// ===================== BOOT button =====================
#define BOOT_BUTTON 0

// ===================== Networking =====================
WiFiUDP udp;
const unsigned int UDP_PORT = 8008;
const int STATIC_HOST_OCTET = 80;
char incomingPacket[16];

IPAddress lastClientIP;
uint16_t  lastClientPort = 0;
bool      haveClient = false;

// ===================== Speed / motion state =====================
int currentSpeed = 80;
const int SPEED_MIN = 80;   // below this the wheels don't reliably *start* turning (measured)
const int SPEED_MAX = 200;
const int SPEED_STEP = 10;
const int WHEEL_MOVING_MIN = 60;  // an already-spinning wheel keeps turning below the static start
                                  // threshold, so correction may briefly dip one wheel to here

enum MotionState { STOPPED, MOVING_FWD, MOVING_BWD, TURNING_L, TURNING_R };
MotionState motionState = STOPPED;
bool obstacleBlocked = false;

// ===================== MPU6050 =====================
MPU6050 mpu;
bool dmpReady = false;
uint8_t fifoBuffer[64];
Quaternion q;
VectorFloat gravity;
float ypr[3];
const int YAW_SIGN = -1; // confirmed: left=neg, right=pos

float targetHeadingDeg = 0.0f;
bool  headingLocked = false;
const float HEADING_DEADZONE_DEG = 1.0f;
const float KP_HEADING_FWD = 3.0f;
const float KP_HEADING_BWD = 4.0f;
const float KD_HEADING     = 4.5f;  // yaw-rate damping (deg/s) - prevents over-correction/oscillation
const int   MAX_TRIM = 80;          // absolute safety ceiling; the dynamic clamp below is usually tighter

// Cached yaw + yaw rate, refreshed once per loop so the DMP FIFO is ALWAYS drained.
// (Draining only while moving lets the FIFO overflow when stopped -> garbage on the
//  first heading lock when you next move off.)
float currentYawDeg   = 0.0f;
bool  yawValid        = false;
float yawRateDps      = 0.0f;       // low-pass filtered angular rate, deg/s
float headingErrorDeg = 0.0f;       // last heading error, for telemetry/tuning

float wrapDeg(float deg) {
  while (deg > 180.0f)  deg -= 360.0f;
  while (deg < -180.0f) deg += 360.0f;
  return deg;
}

// Pump the DMP FIFO and refresh cached yaw + filtered yaw rate. Call every loop.
void updateYaw() {
  if (!dmpReady) return;
  if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    currentYawDeg = YAW_SIGN * (ypr[0] * 180.0f / M_PI);
    yawValid = true;
  }
  // Raw gyro Z is the instantaneous turn rate, available even on loops with no fresh DMP
  // packet. EMA-filter it so sensor noise doesn't make the D term buzz the motors.
  float rate = YAW_SIGN * (mpu.getRotationZ() / 131.0f);  // 131 LSB/(deg/s) at +/-250 dps
  yawRateDps += 0.2f * (rate - yawRateDps);
}

void lockHeadingToCurrent() {
  for (int i = 0; i < 20 && !yawValid; i++) { delay(2); updateYaw(); }
  targetHeadingDeg = currentYawDeg;   // currentYawDeg is refreshed at the top of every loop
  headingLocked = true;
}

// ===================== Encoders (KY-024, 1 magnet/wheel) =====================
const int PIN_ENCODER_L = 34;   // Left wheel KY-024 DO pin
const int PIN_ENCODER_R = 35;   // Right wheel KY-024 DO pin

const unsigned long MIN_PULSE_GAP_US = 5000;     // Debounce
const unsigned long RPM_TIMEOUT_US   = 3000000;  // No pulse 3s -> RPM = 0 (must exceed real period at lowest expected RPM)
const float MAX_PLAUSIBLE_RPM = 150.0f;          // 1 magnet/wheel, 40cm wheel: ~150 RPM = ~11 km/h.
                                                 // Anything faster is sensor chatter, not real speed -> reject.

int lastStateL = HIGH;
unsigned long lastPulseTimeL = 0;
unsigned long lastPulseGapL = 0;

int lastStateR = HIGH;
unsigned long lastPulseTimeR = 0;
unsigned long lastPulseGapR = 0;

// Rolling average of last N pulse gaps, to smooth out async single-pulse jitter
// (with 1 magnet/wheel, L and R update at different moments - comparing raw
// single-sample RPM causes large false "error" spikes that aren't real mismatch)
const int GAP_HISTORY_SIZE = 4;
unsigned long gapHistoryL[GAP_HISTORY_SIZE] = {0};
unsigned long gapHistoryR[GAP_HISTORY_SIZE] = {0};
int gapHistoryIndexL = 0;
int gapHistoryIndexR = 0;
int gapHistoryCountL = 0;
int gapHistoryCountR = 0;

void resetEncoderHistory() {
  gapHistoryIndexL = 0;
  gapHistoryIndexR = 0;
  gapHistoryCountL = 0;
  gapHistoryCountR = 0;
}

float rpmL = 0, rpmR = 0;

// Wheel geometry: diameter 40 cm -> circumference in meters
const float WHEEL_DIAMETER_M = 0.40f;
const float WHEEL_CIRCUMFERENCE_M = PI * WHEEL_DIAMETER_M; // ~1.2566 m

float speedL_mps = 0, speedR_mps = 0;   // linear wheel speed, m/s
float wheelchairSpeed_mps = 0;          // average of both wheels
float wheelchairSpeed_kmh = 0;

// ===================== Encoder-based L/R correction (PI) =====================
const float KP_ENC = 0.4f;     // proportional gain on RPM error (reduced - was overreacting to jitter)
const float KI_ENC = 0.05f;    // integral gain (reduced)
const float ENC_INTEGRAL_LIMIT = 50.0f;
const float ENC_DEADBAND_RPM = 5.0f;  // ignore RPM differences smaller than this
const float ENC_GATE_DEG = 2.0f;      // only equalize wheel RPM when heading error is within this band,
                                      // so the encoder loop never fights an active heading correction
float encIntegral = 0;
int encTrim = 0;

unsigned long lastSpeedCalcTime = 0;
const unsigned long SPEED_CALC_INTERVAL_MS = 100;  // 10 Hz update for RPM/PI/telemetry

// ===================== Motor low-level =====================
void setMotor1(int pwm) { // RIGHT
  pwm = constrain(pwm, -SPEED_MAX, SPEED_MAX);
  if (pwm >= 0) { ledcWrite(RPWM1, pwm); ledcWrite(LPWM1, 0); }
  else          { ledcWrite(LPWM1, -pwm); ledcWrite(RPWM1, 0); }
}
void setMotor2(int pwm) { // LEFT
  pwm = constrain(pwm, -SPEED_MAX, SPEED_MAX);
  if (pwm >= 0) { ledcWrite(RPWM2, pwm); ledcWrite(LPWM2, 0); }
  else          { ledcWrite(LPWM2, -pwm); ledcWrite(RPWM2, 0); }
}
void stopMotors() {
  setMotor1(0);
  setMotor2(0);
  motionState = STOPPED;
  headingLocked = false;
  encIntegral = 0;
  resetEncoderHistory();
  encTrim = 0;
}

// Max trim we can apply at a given commanded speed without driving the fast wheel above
// SPEED_MAX or the slow wheel below WHEEL_MOVING_MIN. Pure function -> unit-testable.
int maxTrimForSpeed(int speed) {
  int m = min(SPEED_MAX - speed, speed - WHEEL_MOVING_MIN);
  if (m < 0) m = 0;
  if (m > MAX_TRIM) m = MAX_TRIM;
  return m;
}

// ===================== Heading-corrected drive =====================
void driveForward() {
  lockHeadingToCurrent();
  motionState = MOVING_FWD;
  encIntegral = 0;
  resetEncoderHistory();
  setMotor1(currentSpeed);
  setMotor2(currentSpeed);
}
void driveBackward() {
  lockHeadingToCurrent();
  motionState = MOVING_BWD;
  encIntegral = 0;
  resetEncoderHistory();
  setMotor1(-currentSpeed);
  setMotor2(-currentSpeed);
}

// Straight-line control. Heading (gyro) is the PRIMARY controller because it directly
// measures what we care about - the actual direction of travel. The encoder loop is
// SUBORDINATE: it only nudges the wheels into RPM balance when the heading is already
// satisfied, so it can never cancel out a deliberate turn-to-correct.
void updateMotionCorrection() {
  if (motionState != MOVING_FWD && motionState != MOVING_BWD) return;

  // ---- Heading trim: PD (P on angle error + D on yaw rate for damping) ----
  float headingTrim = 0;
  float error = 0;
  if (headingLocked && yawValid) {
    error = wrapDeg(currentYawDeg - targetHeadingDeg);
    float eForP = (fabs(error) < HEADING_DEADZONE_DEG) ? 0.0f : error;
    float kp = (motionState == MOVING_FWD) ? KP_HEADING_FWD : KP_HEADING_BWD;
    headingTrim = kp * eForP + KD_HEADING * yawRateDps;
  }
  headingErrorDeg = error; // for telemetry/tuning

  // ---- Encoder trim: only act inside the heading gate, i.e. during true straight cruise.
  // rpmError > 0 means the LEFT wheel is faster -> positive trim speeds the right wheel and
  // slows the left, closing the gap.
  float encTrimF = 0;
  if (fabs(error) < ENC_GATE_DEG) {
    float rpmError = rpmL - rpmR;
    if (fabs(rpmError) < ENC_DEADBAND_RPM) rpmError = 0;  // ignore noise-level mismatch
    encIntegral = constrain(encIntegral + rpmError, -ENC_INTEGRAL_LIMIT, ENC_INTEGRAL_LIMIT);
    encTrimF = KP_ENC * rpmError + KI_ENC * encIntegral;
  } else {
    encIntegral = 0;  // don't wind up while the heading loop owns the correction
  }

  // Symmetric trim: +right / -left keeps the average commanded speed constant.
  float trimF = headingTrim + encTrimF;

  // Dynamic clamp: never push a wheel above SPEED_MAX or below WHEEL_MOVING_MIN. A fixed
  // +/-MAX_TRIM could drive the slow wheel to a standstill at low speed and *worsen* the
  // very mismatch we're correcting.
  int maxTrim = maxTrimForSpeed(currentSpeed);
  int trim = constrain((int)trimF, -maxTrim, maxTrim);
  encTrim = trim; // for telemetry

  int direction = (motionState == MOVING_FWD) ? 1 : -1;
  setMotor1(direction * currentSpeed + trim); // right
  setMotor2(direction * currentSpeed - trim); // left
}

// ===================== Encoder polling + RPM/speed calc =====================
void pollEncoder(int pin, int &lastState, unsigned long &lastPulseTime, unsigned long &lastPulseGap,
                  unsigned long *gapHistory, int &historyIndex, int &historyCount) {
  int currentState = digitalRead(pin);
  unsigned long now = micros();

  if (currentState == LOW && lastState == HIGH) {
    unsigned long gap = now - lastPulseTime;
    // Debounce, then reject impossibly-short gaps (sensor chatter / double-edge near the
    // comparator threshold - the source of the spurious ~124 RPM left-wheel spikes).
    if (gap >= MIN_PULSE_GAP_US && (60000000.0f / gap) <= MAX_PLAUSIBLE_RPM) {
      lastPulseGap = gap;
      lastPulseTime = now;

      gapHistory[historyIndex] = gap;
      historyIndex = (historyIndex + 1) % GAP_HISTORY_SIZE;
      if (historyCount < GAP_HISTORY_SIZE) historyCount++;
    }
  }
  lastState = currentState;
}

// Average of the last few pulse gaps -> smoothed RPM, less sensitive to
// single-sample timing jitter between left/right wheel updates.
float averageGapToRPM(unsigned long *gapHistory, int historyCount, unsigned long lastPulseTime) {
  if (historyCount == 0) return 0;
  if (micros() - lastPulseTime > RPM_TIMEOUT_US) return 0;  // genuinely stopped

  unsigned long sum = 0;
  for (int i = 0; i < historyCount; i++) sum += gapHistory[i];
  float avgGap = (float)sum / historyCount;
  if (avgGap == 0) return 0;
  return 60000000.0f / avgGap;
}


void updateEncodersAndSpeed() {
  pollEncoder(PIN_ENCODER_L, lastStateL, lastPulseTimeL, lastPulseGapL,
              gapHistoryL, gapHistoryIndexL, gapHistoryCountL);
  pollEncoder(PIN_ENCODER_R, lastStateR, lastPulseTimeR, lastPulseGapR,
              gapHistoryR, gapHistoryIndexR, gapHistoryCountR);

  unsigned long now = millis();
  if (now - lastSpeedCalcTime < SPEED_CALC_INTERVAL_MS) return;
  lastSpeedCalcTime = now;

  rpmL = averageGapToRPM(gapHistoryL, gapHistoryCountL, lastPulseTimeL);
  rpmR = averageGapToRPM(gapHistoryR, gapHistoryCountR, lastPulseTimeR);

  speedL_mps = (rpmL / 60.0f) * WHEEL_CIRCUMFERENCE_M;
  speedR_mps = (rpmR / 60.0f) * WHEEL_CIRCUMFERENCE_M;
  wheelchairSpeed_mps = (speedL_mps + speedR_mps) / 2.0f;
  wheelchairSpeed_kmh = wheelchairSpeed_mps * 3.6f;
}

// ===================== Telemetry over UDP (JSON, fixed port) =====================
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 250;
const uint16_t TELEMETRY_PORT = 5003;

void sendTelemetry() {
  if (!haveClient) return;
  unsigned long now = millis();
  if (now - lastTelemetryTime < TELEMETRY_INTERVAL_MS) return;
  lastTelemetryTime = now;

  char buf[200];
  snprintf(buf, sizeof(buf),
           "{\"speed\":%.2f}",
            wheelchairSpeed_kmh);

  udp.beginPacket(lastClientIP, TELEMETRY_PORT);
  udp.print(buf);
  udp.endPacket();
}

// ===================== Ultrasonic =====================
float readDistanceCm(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  unsigned long duration = pulseIn(echoPin, HIGH, 20000);
  if (duration == 0) return 999;
  return duration * 0.0343f / 2.0f;
}

void updateUltrasonics() {
  unsigned long now = millis();

  if (now - lastLeftPing >= PING_INTERVAL_MS) {
    lastLeftPing = now;
    leftDistHistory[leftHistIdx] = readDistanceCm(TRIG_LEFT, ECHO_LEFT);
    leftHistIdx = (leftHistIdx + 1) % US_HISTORY_SIZE;
    // Median-of-3 throws out a single bad pulseIn timeout/stray-echo reading instead of
    // letting it instantly trip or clear the obstacle flag.
    leftDistCm = medianOf3(leftDistHistory[0], leftDistHistory[1], leftDistHistory[2]);
  }
  if (now - lastRightPing >= PING_INTERVAL_MS) {
    lastRightPing = now;
    rightDistHistory[rightHistIdx] = readDistanceCm(TRIG_RIGHT, ECHO_RIGHT);
    rightHistIdx = (rightHistIdx + 1) % US_HISTORY_SIZE;
    rightDistCm = medianOf3(rightDistHistory[0], rightDistHistory[1], rightDistHistory[2]);
  }
}

// Hysteresis: once blocked, distance must climb past OBSTACLE_CLEAR_CM (not just
// back above OBSTACLE_STOP_CM) to clear, so a reading sitting right at the threshold
// can't make the obstacle flag chatter on/off every loop.
bool obstacleAhead(bool currentlyBlocked) {
  float threshold = currentlyBlocked ? OBSTACLE_CLEAR_CM : OBSTACLE_STOP_CM;
  bool clear = (leftDistCm > threshold) && (rightDistCm > threshold);
  return !clear;
}

// ===================== Command handling =====================
void handleCommand(char cmd) {
  switch (cmd) {
    case 'F': case 'f':
      if (obstacleBlocked) break;
      if (motionState == MOVING_BWD) { stopMotors(); break; }
      if (motionState != MOVING_FWD) driveForward();   // ignore repeats so the heading lock persists
      break;

    case 'B': case 'b':
      if (obstacleBlocked) break;
      if (motionState == MOVING_FWD) { stopMotors(); break; }
      if (motionState != MOVING_BWD) driveBackward();  // ignore repeats so the heading lock persists
      break;

    case 'L': case 'l':
      if (obstacleBlocked) break;
      if (motionState != TURNING_L) {
        motionState = TURNING_L;
        headingLocked = false;
        encIntegral = 0;
        resetEncoderHistory();
        setMotor1(currentSpeed);
        setMotor2(-currentSpeed);
      }
      break;

    case 'R': case 'r':
      if (obstacleBlocked) break;
      if (motionState != TURNING_R) {
        motionState = TURNING_R;
        headingLocked = false;
        encIntegral = 0;
        resetEncoderHistory();
        setMotor1(-currentSpeed);
        setMotor2(currentSpeed);
      }
      break;

    case 'S': case 's':
      stopMotors();
      break;

    case 'I':
      currentSpeed = constrain(currentSpeed + SPEED_STEP, SPEED_MIN, SPEED_MAX);
      break;

    case 'D':
      currentSpeed = constrain(currentSpeed - SPEED_STEP, SPEED_MIN, SPEED_MAX);
      break;
  }
}

// ===================== WiFi setup with static last-octet =====================
void setupWiFiStaticOctet() {
  WiFiManager wm;
  wm.setDebugOutput(false);  // suppress WiFiManager's own Serial logging

  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  if (digitalRead(BOOT_BUTTON) == LOW) {
    wm.startConfigPortal("Wheelchair-Setup");
  } else {
    wm.autoConnect("Wheelchair-Setup");
  }

  if (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    ESP.restart();
  }

  IPAddress gw = WiFi.gatewayIP();
  IPAddress subnet = WiFi.subnetMask();
  IPAddress dns = WiFi.dnsIP();

  IPAddress staticIP = gw;
  staticIP[3] = STATIC_HOST_OCTET;

  WiFi.disconnect();
  delay(200);
  WiFi.config(staticIP, gw, subnet, dns);
  WiFi.begin();

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    ESP.restart();
  }
}

// ===================== Setup =====================
void setup() {
  Wire.begin();
  Wire.setClock(400000);

  ledcAttach(LPWM1, PWM_FREQ, PWM_RES_BITS);
  ledcAttach(RPWM1, PWM_FREQ, PWM_RES_BITS);
  ledcAttach(LPWM2, PWM_FREQ, PWM_RES_BITS);
  ledcAttach(RPWM2, PWM_FREQ, PWM_RES_BITS);
  pinMode(EN1, OUTPUT); digitalWrite(EN1, HIGH);
  pinMode(EN2, OUTPUT); digitalWrite(EN2, HIGH);

  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  // Encoders
  pinMode(PIN_ENCODER_L, INPUT);
  pinMode(PIN_ENCODER_R, INPUT);
  lastStateL = digitalRead(PIN_ENCODER_L);
  lastStateR = digitalRead(PIN_ENCODER_R);

  // MPU6050
  mpu.initialize();
  uint8_t devStatus = mpu.dmpInitialize();
  mpu.setXAccelOffset(-3567);
  mpu.setYAccelOffset(-1249);
  mpu.setZAccelOffset(4370);
  mpu.setXGyroOffset(-22);
  mpu.setYGyroOffset(-32);
  mpu.setZGyroOffset(41);
  if (devStatus == 0) {
    mpu.setDMPEnabled(true);
    dmpReady = true;
  }

  setupWiFiStaticOctet();

  udp.begin(UDP_PORT);

  stopMotors();
}

// ===================== Loop =====================
void loop() {
  updateYaw();            // drain DMP FIFO + refresh yaw/yaw-rate every iteration
  updateUltrasonics();
  updateEncodersAndSpeed();

  bool nowBlocked = obstacleAhead(obstacleBlocked);
  if (nowBlocked && !obstacleBlocked) {
    stopMotors();
  }
  obstacleBlocked = nowBlocked;

  int packetSize = udp.parsePacket();
  if (packetSize) {
    lastClientIP = udp.remoteIP();
    lastClientPort = udp.remotePort();
    haveClient = true;

    int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
    if (len > 0) {
      incomingPacket[len] = 0;
      for (int i = 0; i < len; i++) {
        handleCommand(incomingPacket[i]);
      }
    }
  }

  updateMotionCorrection();
  sendTelemetry();
}

/*
 * 냉가스 리펄서 - 1단계 추력 측정 리그 펌웨어 (ESP32)
 *
 * 기능
 *  - HX711 로드셀 80Hz 샘플링 (보드 RATE 핀을 3.3V로 올려야 함, 기본 10Hz면 피크 못 잡음)
 *  - 파일럿 솔레노이드 펄스 구동 (MOSFET 경유)
 *  - 데드맨(암) 스위치: 누르고 있는 동안만 발사 가능
 *  - 시리얼로 CSV 로깅 + 발사 후 피크 추력(N), 총역적(N*s), 상승시간(ms) 자동 계산
 *  - 캘리브레이션 값 NVS 저장 (재부팅해도 유지)
 *
 * 배선
 *  HX711  DOUT -> GPIO16,  SCK -> GPIO4,  VCC -> 3.3V, GND 공통
 *  MOSFET GATE -> GPIO25 (게이트 저항 220옴, 풀다운 10k), 밸브는 12V측
 *  암 스위치   -> GPIO26 - GND (내부 풀업, 누르면 LOW = armed)
 *  상태 LED    -> GPIO2 (온보드)
 *
 * 시리얼 명령 (115200)
 *  t        영점 (tare)
 *  c500     캘리브레이션: 로드셀에 500g 추 올린 상태에서 입력
 *  p100     100ms 펄스 발사 (armed 상태에서만 동작)
 *  s        연속 스트리밍 켜기/끄기 (millis,grams CSV)
 *  ?        현재 상태 출력
 */

#include <Preferences.h>

// ---- 핀 ----
const int PIN_HX_DOUT = 16;
const int PIN_HX_SCK  = 4;
const int PIN_VALVE   = 25;
const int PIN_ARM     = 26;
const int PIN_LED     = 2;

// ---- 안전 한계 ----
const uint32_t MAX_PULSE_MS = 1000;   // 이 이상 펄스 명령은 거부
const uint32_t POST_LOG_MS  = 800;    // 밸브 닫은 뒤 추가 기록 시간

// ---- 상태 ----
Preferences prefs;
float calFactor = 1.0f;    // raw/gram
long  tareOffset = 0;
bool  streaming = false;

// ---- HX711 저수준 (라이브러리 없이 직접, 80Hz 대응) ----
long hxReadRaw() {
  while (digitalRead(PIN_HX_DOUT) == HIGH) { /* 데이터 준비 대기 */ }
  long v = 0;
  noInterrupts();
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_HX_SCK, HIGH);
    delayMicroseconds(1);
    v = (v << 1) | digitalRead(PIN_HX_DOUT);
    digitalWrite(PIN_HX_SCK, LOW);
    delayMicroseconds(1);
  }
  // 25번째 클럭 = 채널A 게인128
  digitalWrite(PIN_HX_SCK, HIGH); delayMicroseconds(1);
  digitalWrite(PIN_HX_SCK, LOW);  delayMicroseconds(1);
  interrupts();
  if (v & 0x800000) v |= 0xFF000000;  // 24bit 부호 확장
  return v;
}

float rawToGrams(long raw) {
  return (float)(raw - tareOffset) / calFactor;
}

bool isArmed() { return digitalRead(PIN_ARM) == LOW; }

void doTare() {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += hxReadRaw();
  tareOffset = sum / 20;
  Serial.println("# tare ok");
}

void doCalibrate(float knownGrams) {
  if (knownGrams <= 0) { Serial.println("# err: c<grams>, ex) c500"); return; }
  long sum = 0;
  for (int i = 0; i < 40; i++) sum += hxReadRaw();
  long avg = sum / 40;
  calFactor = (float)(avg - tareOffset) / knownGrams;
  prefs.putFloat("cal", calFactor);
  Serial.printf("# cal ok factor=%.3f raw/gram\n", calFactor);
}

// ---- 발사 + 기록 ----
void firePulse(uint32_t pulseMs) {
  if (!isArmed()) { Serial.println("# BLOCKED: arm switch not held"); return; }
  if (pulseMs == 0 || pulseMs > MAX_PULSE_MS) {
    Serial.printf("# err: pulse 1..%u ms\n", MAX_PULSE_MS); return;
  }

  Serial.printf("# FIRE %ums\n", pulseMs);
  Serial.println("t_ms,grams");

  const int MAX_SAMPLES = 400;          // 80Hz * ~5s 여유
  static float g[MAX_SAMPLES];
  static uint32_t ts[MAX_SAMPLES];
  int n = 0;

  // 발사 직전 베이스라인 (하우징 자중 등)
  float base = rawToGrams(hxReadRaw());

  uint32_t t0 = millis();
  digitalWrite(PIN_VALVE, HIGH);
  digitalWrite(PIN_LED, HIGH);

  uint32_t tEnd = t0 + pulseMs + POST_LOG_MS;
  bool valveOpen = true;
  while (millis() < tEnd && n < MAX_SAMPLES) {
    // 데드맨: 손 떼면 즉시 닫음
    if (valveOpen && (!isArmed() || millis() - t0 >= pulseMs)) {
      digitalWrite(PIN_VALVE, LOW);
      valveOpen = false;
    }
    long raw = hxReadRaw();           // 80Hz면 약 12.5ms 간격으로 블록됨
    ts[n] = millis() - t0;
    g[n]  = rawToGrams(raw);
    n++;
  }
  digitalWrite(PIN_VALVE, LOW);       // 이중 안전
  digitalWrite(PIN_LED, LOW);

  // 결과 계산
  float peakG = -1e9;
  uint32_t peakT = 0;
  float impulseNs = 0;
  uint32_t riseT = 0;
  bool riseFound = false;

  for (int i = 0; i < n; i++) {
    float f = g[i] - base;
    Serial.printf("%u,%.1f\n", ts[i], f);
    if (f > peakG) { peakG = f; peakT = ts[i]; }
  }
  float peakN = peakG * 9.81f / 1000.0f;

  for (int i = 1; i < n; i++) {
    float f0 = (g[i-1] - base) * 9.81f / 1000.0f;
    float f1 = (g[i]   - base) * 9.81f / 1000.0f;
    float dt = (ts[i] - ts[i-1]) / 1000.0f;
    if (f0 > 0 || f1 > 0) impulseNs += 0.5f * (max(f0,0.0f) + max(f1,0.0f)) * dt;
    if (!riseFound && (g[i]-base) > peakG * 0.5f) { riseT = ts[i]; riseFound = true; }
  }

  Serial.println("# ---- RESULT ----");
  Serial.printf("# peak    : %.2f N (%.0f gf) at t=%ums\n", peakN, peakG, peakT);
  Serial.printf("# impulse : %.3f N*s\n", impulseNs);
  Serial.printf("# rise50  : %ums (valve cmd -> 50%% peak, 밸브 응답 지표)\n", riseT);
  Serial.printf("# samples : %d (%.0f Hz)\n", n, n * 1000.0f / (ts[n-1] ? ts[n-1] : 1));
  Serial.println("# 주의: 샘플레이트가 15Hz 근처로 나오면 HX711 RATE 핀이 아직 10Hz 모드임");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_HX_SCK, OUTPUT);
  pinMode(PIN_HX_DOUT, INPUT);
  pinMode(PIN_VALVE, OUTPUT);
  digitalWrite(PIN_VALVE, LOW);
  pinMode(PIN_ARM, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

  prefs.begin("rig", false);
  calFactor = prefs.getFloat("cal", 1.0f);

  delay(300);
  doTare();
  Serial.println("# repulsor test rig ready");
  Serial.println("# cmds: t=tare  c<g>=calibrate  p<ms>=fire  s=stream  ?=status");
  if (calFactor == 1.0f) Serial.println("# WARNING: 캘리브레이션 안 됨. 추 올리고 c500 실행할 것");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "t") doTare();
    else if (cmd.startsWith("c")) doCalibrate(cmd.substring(1).toFloat());
    else if (cmd.startsWith("p")) firePulse((uint32_t)cmd.substring(1).toInt());
    else if (cmd == "s") { streaming = !streaming; Serial.printf("# stream %s\n", streaming ? "on" : "off"); }
    else if (cmd == "?") {
      Serial.printf("# armed=%d cal=%.3f tare=%ld\n", isArmed(), calFactor, tareOffset);
    }
  }

  if (streaming) {
    long raw = hxReadRaw();
    Serial.printf("%lu,%.1f\n", millis(), rawToGrams(raw));
  }

  // 평시 밸브 강제 닫힘 유지
  if (digitalRead(PIN_VALVE) == HIGH && !isArmed()) digitalWrite(PIN_VALVE, LOW);
}

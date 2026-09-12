import math

# ---------- 상수 ----------
BAR_TO_PA = 1e5
ATM_PA = 101325
R_UNIVERSAL = 8.314
CD = 0.9  # 방출계수

molar_mass  = {"air": 0.02897, "co2": 0.044}
gamma_table = {"air": 1.4,     "co2": 1.3}

# ---------- 입력 ----------
volume_L = float(input("Volume (L): "))
tank_pressure_bar = float(input("Tank Pressure (gauge bar): "))
reg_pressure_bar = float(input("Regulator Exit Pressure (gauge bar): "))
nozzle_diameter_mm = float(input("Nozzle Diameter (mm): "))
temperature_C = float(input("Temperature (C): "))
gas = input("Gas (air/co2): ")

# ---------- SI 변환 ----------
volume_m3 = volume_L / 1000
tank_pressure_pa = tank_pressure_bar * BAR_TO_PA + ATM_PA
reg_pressure_pa = reg_pressure_bar * BAR_TO_PA + ATM_PA
nozzle_diameter_m = nozzle_diameter_mm / 1000
temperature_K = temperature_C + 273.15

# ---------- 기체 물성 ----------
R_specific = R_UNIVERSAL / molar_mass[gas]
gamma = gamma_table[gas]

# ---------- 파생값 ----------
throat_area_m2 = math.pi / 4 * nozzle_diameter_m ** 2
exponent = (gamma + 1) / (2 * (gamma - 1))

# ---------- 계산 함수 ----------
def calc_mdot(P0_pa):
    """절대압 P0를 받아 초킹 질량유량(kg/s) 반환"""
    return CD * throat_area_m2 * P0_pa * math.sqrt(gamma / (R_specific * temperature_K)) * (2 / (gamma + 1)) ** exponent

def calc_thrust(P0_pa):
    """절대압 P0를 받아 추력(N) 반환"""
    mdot = calc_mdot(P0_pa)
    T_t = temperature_K * 2 / (gamma + 1)
    v_t = math.sqrt(gamma * R_specific * T_t)
    P_t = P0_pa * (2 / (gamma + 1)) ** (gamma / (gamma - 1))
    return mdot * v_t + (P_t - ATM_PA) * throat_area_m2

# ---------- 정압 계산 (레귤레이터 기준, 기존 기능) ----------
density = tank_pressure_pa / (R_specific * temperature_K)
mass = density * volume_m3

mdot = calc_mdot(reg_pressure_pa)
thrust = calc_thrust(reg_pressure_pa)
duration = mass / mdot
launches = duration / 0.2

print(f"가스 질량   : {mass * 1000:.1f} g")
print(f"질량유량    : {mdot * 1000:.1f} g/s")
print(f"추력        : {thrust:.1f} N")
print(f"지속시간    : {duration:.1f} 초")
print(f"발사 횟수   : {launches:.1f} 발")

# ---------- 블로우다운 (탱크 직결, 레귤레이터 없음) ----------
print("\n--- 블로우다운 ---")
print("t(s)   P(bar g)   thrust(N)")

dt = 0.1
m_tank = mass
P_tank = tank_pressure_pa
t = 0
step = 0  # 정수 스텝 카운터 (0.5초 간격 출력용: step % 5 == 0)

while P_tank > ATM_PA:
    mdot_now = calc_mdot(P_tank)
    thrust_now = calc_thrust(P_tank)
    if step % 5 == 0:
        print(f"{t:.1f}   {(P_tank - ATM_PA) / BAR_TO_PA:.2f}   {thrust_now:.1f}")
    m_tank = m_tank - mdot_now * dt
    P_tank = m_tank * R_specific * temperature_K / volume_m3
    t += dt
    step += 1
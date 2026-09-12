import math

# ṁ = Cd × At × P0 × sqrt(γ/(R×T0)) × (2/(γ+1))^((γ+1)/(2(γ-1))) 유량 공식

# 단위 변환
BAR_TO_PA = 1e5
ATM_PA = 101325

volume_L = float(input("Volume (L): "))
tank_pressure_bar = float(input("Tank Pressure (gauge bar): "))
reg_pressure_bar = float(input("Regulator Exit Pressure (gauge bar): "))
nozzle_diameter_mm = float(input("Nozzle Diameter (mm): "))
temperature_C = float(input("Temperature (C): "))
gas = input("Gas (air/co2): ")

volume_m3 = volume_L / 1000
tank_pressure_pa = tank_pressure_bar * BAR_TO_PA + ATM_PA
reg_pressure_pa = reg_pressure_bar * BAR_TO_PA + ATM_PA
nozzle_diameter_m = nozzle_diameter_mm / 1000
temperature_K = temperature_C + 273.15

# 기체 상수
R_UNIVERSAL = 8.314
molar_mass = {"air": 0.02897, "co2": 0.044}
R_specific = R_UNIVERSAL / molar_mass[gas]

density = tank_pressure_pa / (R_specific * temperature_K)
mass = density * volume_m3
mass_g = mass * 1000

# γ
gamma_table = {"air": 1.4, "co2": 1.3}
gamma = gamma_table[gas]

CD = 0.9 #방출계수

# 노즐 목 면적
throat_area_m2 = math.pi / 4 * nozzle_diameter_m ** 2

exponent = (gamma + 1) / (2 * (gamma - 1))
mdot = CD * throat_area_m2 * reg_pressure_pa * math.sqrt(gamma / (R_specific * temperature_K)) * (2 / (gamma + 1)) ** exponent

# 목
T_t = temperature_K * 2 / (gamma + 1)
v_t = math.sqrt(gamma * R_specific * T_t)
P_t = reg_pressure_pa * (2 / (gamma + 1)) ** (gamma / (gamma - 1))

thrust = mdot * v_t + (P_t - ATM_PA) * throat_area_m2

duration = mass / mdot
launches = duration / 0.2

print(f"가스 질량   : {mass_g:.1f} g")
print(f"질량유량    : {mdot * 1000:.1f} g/s")
print(f"추력        : {thrust:.1f} N")
print(f"지속시간   : {duration:.1f} 초")
print(f"발사 횟수    : {launches:.1f} 발")
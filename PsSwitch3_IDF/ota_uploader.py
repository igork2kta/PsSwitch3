import re
import time
import requests

ESP_IP = "192.168.0.210"

FIRMWARE_PATH = "build/PsSwitch3.bin"
GLOBAL_H_PATH = "components/config/Global.h"

UPDATE_URL = f"http://{ESP_IP}/update"
VERSION_URL = f"http://{ESP_IP}/api/user/lights/1"


def get_sw_version():
    with open(GLOBAL_H_PATH, "r", encoding="utf-8") as f:
        content = f.read()

    match = re.search(r'#define\s+SW_VERSION\s+"([^"]+)"', content)

    if not match:
        raise RuntimeError("SW_VERSION não encontrada em Global.h")

    return match.group(1)


expected_version = get_sw_version()

print(f"Versão esperada: {expected_version}")
print(f"Enviando: {FIRMWARE_PATH}")
print(f"Destino: {UPDATE_URL}")

with open(FIRMWARE_PATH, "rb") as f:
    firmware_data = f.read()

response = requests.put(
    UPDATE_URL,
    data=firmware_data,
    headers={
        "Content-Type": "application/octet-stream",
        "Content-Length": str(len(firmware_data))
    },
    timeout=120
)

print()
print("Status do upload:", response.status_code)
print("Resposta:", response.text)

if response.status_code != 200:
    exit(1)

print("\nAguardando o ESP reiniciar...")
time.sleep(10)

print(f"Consultando {VERSION_URL}")

for tentativa in range(10):
    try:
        response = requests.get(VERSION_URL, timeout=10)
        response.raise_for_status()

        data = response.json()
        device_version = data.get("swversion")

        print(f"Versão do dispositivo: {device_version}")

        if device_version == expected_version:
            print("✅ Firmware atualizado com sucesso.")
        else:
            print("❌ Versão incorreta!")
            print(f"Esperado : {expected_version}")
            print(f"Recebido : {device_version}")

        break

    except Exception as e:
        if tentativa == 9:
            print("Não foi possível consultar a versão do dispositivo.")
            print(e)
        else:
            time.sleep(2)
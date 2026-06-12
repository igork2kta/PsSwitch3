import requests

ESP_IP = "192.168.0.126"

FIRMWARE_PATH = "build/portal-wifi.bin"

url = f"http://{ESP_IP}/update"

print(f"Enviando: {FIRMWARE_PATH}")
print(f"Destino: {url}")

with open(FIRMWARE_PATH, "rb") as f:

    firmware_data = f.read()

response = requests.put(
    url,
    data=firmware_data,
    headers={
        "Content-Type": "application/octet-stream",
        "Content-Length": str(len(firmware_data))
    },
    timeout=120
)

print()
print("Status:", response.status_code)
print("Resposta:", response.text)
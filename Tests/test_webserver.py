import requests
import pytest

# pytest test_esp8266.py

# Substitua pelo IP real do seu ESP8266 na rede
ESP_IP = "192.168.0.126" 
BASE_URL = f"http://{ESP_IP}"

def test_root_page():
    """Valida se a página de provisionamento está acessível"""
    response = requests.get(f"{BASE_URL}/")
    assert response.status_code == 200
    # Verifica se é um GZIP (conforme seu server.sendHeader("Content-Encoding", "gzip"))
    assert response.headers.get("Content-Encoding") == "gzip"

def test_hue_api_auth():
    """Valida o endpoint de pareamento /api"""
    response = requests.post(f"{BASE_URL}/api")
    assert response.status_code == 200
    data = response.json()
    assert "success" in data[0]
    assert "username" in data[0]["success"]

def test_light_state_toggle():
    """Testa ligar e desligar a luz via PUT"""
    url = f"{BASE_URL}/api/12345/lights/1/state"
    
    # Testar Ligar com brilho 150
    payload = '{"on": true, "bri": 150}' # Enviando como string para simular o getBody
    response = requests.put(url, data=payload)
    assert response.status_code == 200
    
    # Validar se o estado mudou fazendo um GET
    get_res = requests.get(url)
    assert "\"on\": true" in get_res.text
    assert "\"bri\": 150" in get_res.text


def test_rename_device():
    """Testa a função de renomear o dispositivo"""
    new_name = "ESP TEST"
    url = f"{BASE_URL}/api/12345/lights/1/rename"
    payload = f'{{"name":"{new_name}"}}'
    
    response = requests.put(url, data=payload)
    assert response.status_code == 200
    assert "Sucesso!" in response.text

    # Verificar se o nome foi atualizado
    url = f"{BASE_URL}/api/12345/lights/1"
    response = requests.get(url)
    assert response.status_code == 200
    assert new_name in response.text


def test_invalid_reset_key():
    """Garante que o reset falha com a chave errada"""
    url = f"{BASE_URL}/api/12345/lights/1/reset"
    payload = '{"key":"ERRO"}'
    response = requests.put(url, data=payload)
    assert response.status_code == 403
    assert "Unauthorized" in response.text


if __name__ == '__main__':
    import pytest
    pytest.main([__file__])
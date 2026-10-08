import gzip
import os
import re
import shutil
import subprocess
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))

TEMP_DIR = os.path.join(BASE_DIR, "temp")

# Projeto ESP-IDF
IDF_WEB_DIR = os.path.abspath(
    os.path.join(
        BASE_DIR,
        "..",
        "PsSwitch3_IDF",
        "components",
        "webserver"
    )
)

NODE_SCRIPT = os.path.join(BASE_DIR, "minifier.js")


FILES = [
    "provisioning_page.html",
    "web_interface.html",
    "web_interface_wol.html",
]


def executar_node():
    """
    Executa o script Node.js responsável por:
    - minificar HTML
    - comprimir com Gzip
    - gerar os arquivos .gz na pasta temp
    """

    print("=" * 60)
    print("1. MINIFICANDO E COMPRIMINDO")
    print("=" * 60)

    os.makedirs(TEMP_DIR, exist_ok=True)

    # Limpa os arquivos .gz antigos
    for file_name in FILES:
        gz_path = os.path.join(TEMP_DIR, file_name + ".gz")

        if os.path.exists(gz_path):
            os.remove(gz_path)

    print(f"Node.js: {NODE_SCRIPT}")
    print(f"Temp:    {TEMP_DIR}\n")

    try:
        subprocess.run(
            [
                "node",
                NODE_SCRIPT,
                TEMP_DIR
            ],
            check=True
        )

    except FileNotFoundError:
        print("❌ Node.js não foi encontrado.")
        print("Verifique se 'node' está no PATH.")
        sys.exit(1)

    except subprocess.CalledProcessError as e:
        print(f"❌ O Node.js terminou com erro. Código: {e.returncode}")
        sys.exit(e.returncode)


def copiar_para_espidf():
    """
    Copia os arquivos .gz gerados pelo Node
    para o projeto ESP-IDF.
    """

    print("\n" + "=" * 60)
    print("2. COPIANDO PARA ESP-IDF")
    print("=" * 60)

    os.makedirs(IDF_WEB_DIR, exist_ok=True)

    print(f"Destino: {IDF_WEB_DIR}\n")

    for file_name in FILES:

        gz_name = file_name + ".gz"

        source = os.path.join(TEMP_DIR, gz_name)
        destination = os.path.join(IDF_WEB_DIR, gz_name)

        if not os.path.exists(source):
            print(f"❌ Arquivo não encontrado: {source}")
            continue

        shutil.copy2(source, destination)

        print(f"✅ {gz_name}")


def gerar_header(gz_filename, output_h_filename):
    """
    Converte um .gz em um header C/C++ no formato
    semelhante ao xxd -i.
    """

    gz_path = os.path.join(TEMP_DIR, gz_filename)
    h_path = os.path.join(TEMP_DIR, output_h_filename)

    if not os.path.exists(gz_path):
        print(f"❌ Arquivo não encontrado: {gz_path}")
        return

    print(f"\n🔧 Gerando {output_h_filename}")

    with open(gz_path, "rb") as f:
        gz_data = f.read()

    # Nome da variável
    var_name = re.sub(
        r'[^a-zA-Z0-9]',
        '_',
        gz_filename
    )

    # xxd -i normalmente usa unsigned char
    hex_lines = []

    bytes_por_linha = 12

    for i in range(0, len(gz_data), bytes_por_linha):

        chunk = gz_data[i:i + bytes_por_linha]

        hex_chunk = ", ".join(
            f"0x{b:02x}"
            for b in chunk
        )

        hex_lines.append("    " + hex_chunk)

    hex_content = ",\n".join(hex_lines)

    header = f"""#pragma once


const char {var_name}[] PROGMEM = {{
{hex_content}
}};

constexpr unsigned int {var_name}_len = {len(gz_data)};
"""

    with open(h_path, "w", encoding="utf-8") as f:
        f.write(header)

    print(f"   Tamanho: {len(gz_data)} bytes")
    print(f"   📁 {h_path}")


def gerar_headers_arduino():
    """
    Gera os headers .h para o ESP8266/Arduino.
    """

    print("\n" + "=" * 60)
    print("3. GERANDO HEADERS PARA ESP8266")
    print("=" * 60)

    for file_name in FILES:

        gz_name = file_name + ".gz"

        # provisioning_page.html -> provisioning_page.h
        # web_interface.html     -> web_interface.h
        h_name = os.path.splitext(file_name)[0] + ".h"

        gerar_header(
            gz_name,
            h_name
        )


def main():

    print("\n")
    print("=" * 60)
    print("       BUILD DOS ARQUIVOS WEB")
    print("=" * 60)
    print()

    executar_node()

    copiar_para_espidf()

    gerar_headers_arduino()

    print("\n" + "=" * 60)
    print("BUILD CONCLUÍDO")
    print("=" * 60)

    print(f"\n📁 Arquivos temporários:")
    print(f"   {TEMP_DIR}")

    print(f"\n📁 Arquivos ESP-IDF:")
    print(f"   {IDF_WEB_DIR}")

    print("\n")


if __name__ == "__main__":
    main()
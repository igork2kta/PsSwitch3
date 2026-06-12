import gzip
import os
import re


#TUTORIAL MANUAL
    #COMPACTAR O ARQUIVO PARA GZ, PODE USAR O 7ZIP  MESMO, COMPRESSÃO MÁXIMA
    #NO LINUX xxd -i web_interface.html.gz > web_interface.h
    #Lembrar de colocar PROGMEM e const na frente do array: const unsigned char web_interface_html_gz[] PROGMEM = {


def html_para_header_comprimido(html_filename, output_h_filename):
    """
    Compacta um arquivo HTML com compressão máxima (Gzip)
    e gera um arquivo de cabeçalho (.h) no padrão do comando 'xxd -i'.
    """
    if not os.path.exists(html_filename):
        print(f"Erro: O arquivo '{html_filename}' não foi encontrado.")
        return

    print(f"1. Lendo '{html_filename}' e aplicando compressão máxima (Gzip nível 9)...")
    with open(html_filename, 'rb') as f_in:
        html_data = f_in.read()
    
    # Nome virtual que o xxd usaria como base para as variáveis
    gz_virtual_name = html_filename + ".gz"
    
    # Aplica compressão máxima (compresslevel=9)
    gz_data = gzip.compress(html_data, compresslevel=9)
    gz_len = len(gz_data)
    
    print("2. Formatando os bytes no padrão C/C++ ('xxd -i')...")
    # O xxd substitui caracteres especiais (como pontos) por underlines
    var_name = re.sub(r'[^a-zA-Z0-9]', '_', gz_virtual_name)
    
    # Organiza em linhas de 12 bytes para o código ficar legível (idêntico ao xxd)
    hex_lines = []
    bytes_por_linha = 12
    for i in range(0, gz_len, bytes_por_linha):
        chunk = gz_data[i:i+bytes_por_linha]
        hex_chunk = ", ".join(f"0x{b:02x}" for b in chunk)
        hex_lines.append("  " + hex_chunk)
        
    # Junta todas as linhas separando por vírgula e quebra de linha
    hex_content = ",\n".join(hex_lines)
    
    # Monta a estrutura final do arquivo .h
    header_template = f"""const char {var_name}[] PROGMEM= {{
{hex_content}
}};
constexpr unsigned int {var_name}_len = {gz_len};
"""

    # Grava o arquivo de cabeçalho
    with open(output_h_filename, 'w', encoding='utf-8') as f_out:
        f_out.write(header_template)
        
    print(f"\nSucesso! Arquivo '{output_h_filename}' gerado.")
    print(f"-> Tamanho original: {len(html_data)} bytes")
    print(f"-> Tamanho compactado: {gz_len} bytes")

if __name__ == "__main__":
    # Defina aqui os nomes dos seus arquivos
    arquivo_html = "web_interface.html"
    arquivo_header = "web_interface.h"
    
    # Cria um HTML de teste apenas se ele não existir na pasta
    if not os.path.exists(arquivo_html):
        print(f"'{arquivo_html}' não encontrado.")
        exit(1)
    
    html_para_header_comprimido(arquivo_html, arquivo_header)
# Guia de Configuração: Flash Size e Tabela de Partições no ESP-IDF

Este guia instrui como resolver o erro de tamanho de flash (`Partitions tables occupies X MB... does not fit in configured flash size`) e como configurar uma **Tabela de Partições Customizada com suporte a OTA** para **ESP32** e **ESP32-S3** utilizando o ESP-IDF.

---

## 1. Diagnóstico do Problema

Ao compilar o firmware no ESP-IDF, se a soma dos tamanhos definidos na tabela de partições (`partitions.csv`) for maior do que o **Flash Size** configurado no `menuconfig`, o sistema exibirá um erro como:

```text
Partitions tables occupies 3.1MB of flash (3276800 bytes) which does not fit in configured flash size 2MB.
Change the flash size in menuconfig under the 'Serial Flasher Config' menu.
```

Por padrão, alguns projetos no ESP-IDF vêm pré-configurados com **2 MB** de Flash. No entanto, a maioria das placas de desenvolvimento possui:
- **ESP32 padrão:** 4 MB (ou 8 MB/16 MB em versões específicas).
- **ESP32-S3:** 4 MB, 8 MB ou 16 MB (além da opção Quad/Octal SPI).

---

## 2. Passo a Passo: Ajustar o Flash Size no `menuconfig`

Siga as etapas abaixo para ajustar o tamanho da Flash de acordo com o hardware utilizado:

1. No terminal do seu projeto ESP-IDF, abra a ferramenta de configuração:
   ```bash
   idf.py menuconfig
   ```

2. Navegue no menu até:
   **`Serial flasher config`** -> **`Flash size`**

3. Escolha a capacidade da memória Flash física do seu chip:
   - **4 MB:** Padrão para a grande maioria dos módulos ESP32 (WROOM-32) e ESP32-S3 básicos.
   - **8 MB:** Comum em módulos ESP32-S3 (ex: ESP32-S3-DevKitC-1 com N8R8).
   - **16 MB:** Módulos de alta capacidade (ex: N16R8).

4. *(Apenas para ESP32-S3 - Opcional / Recomendado)*:
   - Verifique a opção **`Flash SPI mode`** (DIO/QIO para Quad SPI ou OPI para Octal SPI, dependendo da variante do chip S3).

5. Salve a configuração pressionando `S`, confirme o salvamento e saia pressionando `Esc`.

---

## 3. Configurando a Tabela de Partições (`partitions.csv`)

Crie ou edite o arquivo **`partitions.csv`** na raiz do seu projeto ESP-IDF.

### Estrutura dos Campos:
- **Name:** Nome legível da partição.
- **Type:** Tipo principal (`data` para dados, `app` para programas executáveis).
- **SubType:** Subtipo (`nvs`, `ota`, `phy` para dados; `factory`, `ota_0`, `ota_1` para app).
- **Offset:** Endereço de memória inicial (pode ser deixado em branco para cálculo automático após o offset anterior).
- **Size:** Tamanho da partição (ex: `0x5000`, `1M`, `1900K`, `3M`).

---

### Opção A: Tabela OTA Recomendada para Flash de 4 MB (Com `factory`)

Ideal se você deseja manter uma versão de fábrica (*factory*) intocável para restauração e duas áreas para atualização OTA (`ota_0` e `ota_1`).

```csv
# Name,    Type, SubType, Offset,  Size
nvs,       data, nvs,     0x9000,  0x5000
otadata,   data, ota,     0xe000,  0x2000
phy_init,  data, phy,     0x10000, 0x1000

factory,   app,  factory, 0x20000, 1M
ota_0,     app,  ota_0,   ,        1M
ota_1,     app,  ota_1,   ,        1M
```

> **Nota:** Limita o tamanho máximo do seu arquivo binário do firmware a **1 MB** (1048576 bytes).

---

### Opção B: Tabela OTA Otimizada para Flash de 4 MB (Sem `factory` - Mais Espaço)

Se o seu código for maior que 1 MB (devido ao uso de Wi-Fi, Bluetooth, HTTPS, WebServer, etc.), remova a partição `factory`. O ESP-IDF inicializará diretamente a partir do slot `ota_0`.

```csv
# Name,    Type, SubType, Offset,  Size
nvs,       data, nvs,     0x9000,  0x5000
otadata,   data, ota,     0xe000,  0x2000
phy_init,  data, phy,     0x10000, 0x1000

ota_0,     app,  ota_0,   0x20000, 1900K
ota_1,     app,  ota_1,   ,        1900K
```

> **Nota:** Aumenta o limite de cada firmware para **~1.85 MB** (1900 KB).

---

### Opção C: Tabela OTA para Flash de 8 MB ou 16 MB (ESP32 / ESP32-S3)

Para placas com maior capacidade de armazenamento, você pode atribuir até 3 MB ou 4 MB por slot de aplicação:

```csv
# Name,    Type, SubType, Offset,  Size
nvs,       data, nvs,     0x9000,  0x5000
otadata,   data, ota,     0xe000,  0x2000
phy_init,  data, phy,     0x10000, 0x1000

factory,   app,  factory, 0x20000, 2M
ota_0,     app,  ota_0,   ,        3M
ota_1,     app,  ota_1,   ,        3M
```

---

## 4. Habilitar o `partitions.csv` Customizado no ESP-IDF

Depois de criar/editar o arquivo `partitions.csv` na raiz do seu projeto:

1. Abra o `menuconfig`:
   ```bash
   idf.py menuconfig
   ```
2. Vá em:
   **`Partition Table`** -> **`Partition Table`**
3. Altere de *Single factory app, no OTA* para:
   **`Custom partition table CSV`**
4. Verifique o campo **`Custom partition CSV file`**:
   Garanta que o nome está definido como **`partitions.csv`**.
5. Salve (`S`) e saia (`Esc`).

---

## 5. Recompilar e Gravar o Firmware via USB

Como a alteração da tabela de partições redefine a estrutura física da memória Flash, a primeira atualização **DEVE ser gravada obrigatoriamente via cabo USB**:

1. Limpe a compilação antiga (recomendado):
   ```bash
   idf.py fullclean
   ```
2. Recompile o projeto:
   ```bash
   idf.py build
   ```
3. Grave o firmware e a nova tabela de partições via USB:
   ```bash
   idf.py -p COMX flash   # Substitua COMX pela porta serial (ex: COM3 no Windows)
   ```

Após esse processo via USB, o ESP32/ESP32-S3 estará pronto para receber atualizações via HTTP/HTTPS OTA através do seu script Python.
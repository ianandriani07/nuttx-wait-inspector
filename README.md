# NuttX Wait Inspector

Trabalho da disciplina de Sistemas Operacionais Embarcados da UFSC.

O projeto adiciona ao Apache NuttX a função `sem_get_wait_info()`, que permite consultar as tarefas bloqueadas na fila de um semáforo específico.

A função informa:

- Quantas tarefas estão aguardando.
- Quantas têm prioridade atual maior ou igual a um limiar.
- Qual é a maior prioridade entre os aguardantes.

A implementação e a aplicação de testes foram executadas em uma **ESP32-S3 N8R2**, usando **NuttX 13.1.1 em modo FLAT**.

## Como funciona

A interface pública está declarada em `<semaphore.h>`:

```c
struct sem_wait_info_s
{
    uint32_t total_waiters;
    uint32_t waiters_ge_threshold;
    int max_priority;
};

int sem_get_wait_info(FAR sem_t *sem, int min_priority,
                      FAR struct sem_wait_info_s *info);
```

`sem` deve apontar para um semáforo inicializado, e `info` deve apontar para a estrutura que receberá o resultado.

O limiar `min_priority` aceita valores de **1 a 255**, inclusive.

| Campo | Significado |
|---|---|
| `total_waiters` | Total de tarefas na fila do semáforo |
| `waiters_ge_threshold` | Tarefas cuja prioridade atual é maior ou igual ao limiar |
| `max_priority` | Maior prioridade entre todos os aguardantes, ou `-1` se a fila estiver vazia |

Por exemplo, para três tarefas aguardando com prioridades **110, 120 e 130**, uma consulta com limiar **120** retorna:

```text
total_waiters = 3
waiters_ge_threshold = 2
max_priority = 130
```

A função retorna `0` no sucesso. Retorna `-1` e define `errno = EINVAL` quando algum dos ponteiros é `NULL` ou o limiar está fora da faixa válida. Nesses erros, a estrutura de resultado permanece inalterada.

A consulta percorre a fila dentro de uma seção crítica, sem consumir o semáforo ou despertar tarefas. O resultado representa o estado observado durante a consulta e pode ficar desatualizado depois que a função retorna.

### Interface pública e função interna

A aplicação chama `sem_get_wait_info()`. Esse wrapper chama `nxsem_get_wait_info()`, que faz a consulta e retorna erros negativos. O wrapper converte esses erros para o padrão público de retorno `-1` com `errno`.

A função interna também foi registrada em `syscall.csv` e `syscall_lookup.h`, seguindo o padrão das interfaces `nxsem_*` desta versão do NuttX.

**No modo FLAT usado neste projeto, a chamada é direta.** O registro formal não significa que uma transição de privilégio tenha sido testada. Os modos PROTECTED e KERNEL não foram validados neste trabalho.

## Organização do repositório

```text
nuttx-wait-inspector/
├── nuttx/       Código-fonte do NuttX e alterações no kernel
├── apps/        Aplicações do NuttX, incluindo o wait_inspector
├── configs/
│   ├── esp32s3-n8r2.defconfig
│   └── esp32s3-n8r2-wait-inspector.defconfig
└── toolchains/  Compilador instalado localmente; ignorado pelo Git
```

O primeiro defconfig preserva a configuração original da placa. O segundo habilita a aplicação de testes.

O repositório não inclui toolchain, ambiente virtual Python, firmware compilado ou arquivos temporários de build.

## Preparação do ambiente

Os comandos de instalação abaixo são destinados a Ubuntu/Debian em Linux x86_64, com Python 3.10 ou superior.

**Se o seu ambiente já está configurado e compilando este projeto, pule esta preparação e vá para “Compilar novamente”.** Não é necessário reinstalar as ferramentas.

### 1. Instalar as ferramentas e obter o projeto

```bash
sudo apt update
sudo apt install build-essential git curl xz-utils python3 python3-venv \
    gperf genromfs xxd picocom
```

Clone o repositório e entre na pasta:

```bash
git clone https://github.com/ianandriani07/nuttx-wait-inspector.git
cd nuttx-wait-inspector
```

Os próximos comandos devem ser executados na raiz do repositório.

### 2. Preparar as ferramentas Python

Para um ambiente novo:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install "kconfiglib==14.1.0" "esptool==5.6.0"
```

Essas são as versões utilizadas durante os testes.

Se você já possui um ambiente virtual com essas ferramentas, pode ativá-lo em vez de criar outro. Na máquina usada no desenvolvimento, ele fica em:

```bash
source ~/nuttxspace/.venv/bin/activate
```

Use apenas uma das opções de ambiente virtual.

### 3. Instalar a toolchain

Foi utilizada a toolchain da Espressif **GCC 14.2.0**, versão **esp-14.2.0_20241119**.

Download original: [release da Espressif](https://github.com/espressif/crosstool-NG/releases/tag/esp-14.2.0_20241119).

Se ela ainda não estiver instalada:

```bash
mkdir -p toolchains

curl -fL -o toolchains/xtensa-esp-elf.tar.xz \
    https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20241119/xtensa-esp-elf-14.2.0_20241119-x86_64-linux-gnu.tar.xz

tar -xJf toolchains/xtensa-esp-elf.tar.xz -C toolchains
```

Adicione o compilador ao terminal atual e confira a versão:

```bash
export PATH="$PWD/toolchains/xtensa-esp-elf/bin:$PATH"
xtensa-esp32s3-elf-gcc --version
```

Se já possui essa toolchain em outro local, ajuste o caminho no `export`.

### 4. Aplicar a configuração em um clone novo

Execute este bloco apenas na primeira configuração de um clone ainda não configurado:

```bash
(
    cd apps/examples &&
    ../tools/mkkconfig.sh -m "Examples"
)

cp configs/esp32s3-n8r2-wait-inspector.defconfig nuttx/.config

ln -s boards/xtensa/esp32s3/esp32s3-devkit/scripts/Make.defs \
    nuttx/Make.defs

make -C nuttx olddefconfig
```

O script atualiza a lista de exemplos. A cópia aplica o defconfig do projeto, e o link seleciona as regras de compilação da placa.

A configuração salva já habilita o comando `wait_inspector`; não é necessário procurar a opção no menu.

## Compilar novamente

Na raiz do repositório, ative o ambiente Python e disponibilize a toolchain no terminal atual:

```bash
source .venv/bin/activate
export PATH="$PWD/toolchains/xtensa-esp-elf/bin:$PATH"
```

Se estiver reutilizando o ambiente original de desenvolvimento, use `source ~/nuttxspace/.venv/bin/activate` no lugar da primeira linha.

Compile:

```bash
make -C nuttx -j4
```

A ESP32-S3 não precisa estar conectada durante a compilação.

Na primeira compilação, mantenha acesso à internet: o build obtém a HAL da Espressif e seus submódulos. Essa dependência não é versionada neste repositório.

Ao terminar com sucesso, o build informa:

```text
Generated: nuttx.bin
```

O firmware estará em:

```text
nuttx/nuttx.bin
```

## Gravar na ESP32-S3

**A partir desta etapa, conecte a placa por USB.**

Os testes utilizaram `/dev/ttyUSB0`. Se sua placa aparecer em outra porta, substitua esse caminho nos comandos. Você pode consultar os dispositivos disponíveis com:

```bash
ls -l /dev/serial/by-id/
```

Feche qualquer console serial que esteja usando a porta. Para sair do picocom, pressione **Ctrl+A** e depois **Ctrl+X**.

Com o ambiente Python ativo e na raiz do repositório:

```bash
python -m esptool \
    --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
    write-flash --flash-size 8MB --flash-mode dio --flash-freq 40m \
    0x0 nuttx/nuttx.bin
```

O endereço `0x0` corresponde à configuração **simple boot** usada neste projeto.

A gravação deve terminar com:

```text
Hash of data verified.
```

## Executar os testes

Abra o console serial:

```bash
picocom -b 115200 /dev/ttyUSB0
```

Pressione **Enter** para obter o prompt `nsh>`. Se não aparecer, pressione o botão **RESET/EN** da placa com o console aberto.

No NSH, execute:

```text
wait_inspector
```

A saída esperada é:

```text
PASS: threshold=1 total=0 ge=0 max=-1
PASS: threshold=255 total=0 ge=0 max=-1
PASS: NULL semaphore
PASS: NULL result
PASS: threshold 0
PASS: threshold 256
PASS: threshold=1 total=3 ge=3 max=130
PASS: threshold=110 total=3 ge=3 max=130
PASS: threshold=120 total=3 ge=2 max=130
PASS: threshold=130 total=3 ge=1 max=130
PASS: threshold=131 total=3 ge=0 max=130
PASS: threshold=255 total=3 ge=0 max=130
PASS: threshold=120 total=0 ge=0 max=-1
wait_inspector: PASS (0 failures)
```

A aplicação verifica a fila vazia, os erros de entrada e a preservação do resultado nos erros. Depois cria três threads com prioridades explícitas **110, 120 e 130**.

Antes das consultas, confirma com `sem_getvalue()` que existem três aguardantes. Ao terminar, libera as threads, aguarda sua finalização e verifica que a fila voltou a ficar vazia.

Qualquer resultado `FAIL` ou resumo com falhas indica que o teste não passou.

## Onde estão as alterações

| Arquivo ou diretório | Alteração |
|---|---|
| `nuttx/include/semaphore.h` | Estrutura de resultado e declaração pública |
| `nuttx/include/nuttx/semaphore.h` | Declaração da função interna |
| `nuttx/sched/semaphore/sem_get_wait_info.c` | Consulta protegida da fila de espera |
| `nuttx/libs/libc/semaphore/sem_get_wait_info.c` | Wrapper público e conversão de erros |
| `nuttx/sched/semaphore/Make.defs` | Inclusão da função interna no build |
| `nuttx/libs/libc/semaphore/Make.defs` | Inclusão do wrapper no build |
| `nuttx/syscall/syscall.csv` | Descrição da assinatura da syscall interna |
| `nuttx/include/sys/syscall_lookup.h` | Entrada da syscall com três argumentos |
| `apps/examples/wait_inspector/` | Aplicação de testes e arquivos de configuração/build |
| `configs/esp32s3-n8r2-wait-inspector.defconfig` | Configuração reproduzível com os testes habilitados |

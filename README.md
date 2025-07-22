# Atividade 28-04

Este é o repositório que armazena a tarefa solicitada no dia 17/07. Todos os arquivos necessários à execução já foram criados, de modo que basta seguir as instruções abaixo para executá-lo em seu dispositivo.

## Como Usar

1. Para acessar a atividade aramzenada, clone este repositório para seu dispositivo executando o seguinte comando no terminal de um ambiente adequado, como o VS Code, após criar um repositório local: 
git clone https://github.com/nivaldojunior037/Atividade-22-07-EmbarcaTech

2. Após isso, importe como um projeto a pasta que contém os arquivos clonados em um ambiente como o VS Code e compile o código existente para que os demais arquivos necessários sejam criados em seu dispositivo

3. Ao fim da compilação, será gerado um arquivo .uf2 na pasta build do seu repositório. Esse arquivo deve ser copiado para a BitDogLab em modo BOOTSEL para que ele seja corretamente executado. 

4. É necessário conectar um sensor BMP280 ou um AHT20 ao terminal I2C0 da BitDogLab para que o código funcione adequadamente. 

### Como Executar o Código

1. Para executar o código, basta manter a conexão dos sensores com a BitDogLab enquanto se desejar realizar a medição. Os valores medidos podem ser atestados no display da placa, no monitor serial e na página web. 

2. A matriz de LEDS, o LED RGB central e o buzzer emitem feedback, indicando se os valores medidos se aproximam ou ultrapassam os valores de limites predefinidos ou redefinidos na página web. 

3. É possível realizar o monitoramento serial e verificar as medições, bastando utilizar uma extensão adequada, como o VS Code com baud rate de 115200.

#### Link do vídeo

Segue o link do Drive com o vídeo onde é demonstrada a utilização do código: https://drive.google.com/drive/folders/1cG0SWvqzQWwAn2GwoQhjBoJyQtpQ7dHG?usp=sharing
# Subscriber

> Subscriber MQTT que se inscreve para receber mensagens do tópico 'cefet/iot'

## Como funciona?

> Para executar o projeto, primeiro é preciso criar e iniciar o ambiente virtual

```bash
python -m venv env # criação do ambiente
source ./env/bin/activate # ativação do ambiente virtual
```

> Após isso, é preciso criar o banco de dados com as tabelas que irão armazenar as informações vindas das mensagens. Para isso, rode o comando:

```bash
python main.py migrate
```

> Isso irá criar o banco SQLite iot.db com as tabelas dispositivos(id, lat, long) e medidas(id, temperatura, umidade, timestamp, id_dispositivo). Após isso, basta rodar o comando:

```bash
python main.py
```

> que o subscriber ficará esperando por mensagens do tópico 'cefet/iot'

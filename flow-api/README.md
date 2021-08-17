<h6>TCC</h6>
<h1 align="center"> Sistema de Sensoriamento de água </h1>
<h6 align="center">by Keiko</h6>

<h3> FLOW API </h3>

Para realizar a integração com sistemas de gerenciamento condominial utilizamos uma API desenvolvida em node js que faz a requisição das consultas definidas.

<h3> INSTALAÇÃO (é necessário ter instalado o nodejs em sua máquina)</h3>

Clone o projeto:

```
git clone https://github.com/keikomori/flow-sys/flow-api
```

Acesse a pasta api:

```
cd api
```

Inicialize o node no projeto:

```
npm init
```

Instale o influx:

```
npm install --save influx
```

Instale o express:

```
npm install express --save
```

Execute a api:

```
node server.js
```


<h4>Referências:</h4>

https://node-influx.github.io/

https://www.npmjs.com/package/influxdb-nodejs

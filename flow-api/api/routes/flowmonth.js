const express = require('express');
const Influx = require('influx');
const router = express.Router();

const influx= new Influx.InfluxDB({
    host: 'localhost',
    database: 'hydra',
    user: 'telegraf',
    password: 'password',
    port:8086
    });


router.get('/', (req, res, next) => {
    influx.query(
        `select last(consumer) - first(consumer) from mqtt_consumer where time >= '2021-08-01T21:06:50.136Z' and time < now()`
        )
        .catch(err=>{
        console.log(err);
        })
        .then(results=>{
        res.json(results);
        });
});

module.exports = router;
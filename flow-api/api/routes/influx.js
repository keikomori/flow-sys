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
        `select * from mqtt_consumer`
        )
        .catch(err=>{
        console.log(err);
        })
        .then(results=>{
        res.json(results);
        });
});

module.exports = router;
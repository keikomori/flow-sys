const express = require('express');
const app = express();

const routeConsumerFlowMonth = require('./routes/flowmonth');
const routeConsumerInflux = require('./routes/influx');

app.use('/flowmonth', routeConsumerFlowMonth)
app.use('/influx', routeConsumerInflux)

module.exports = app;
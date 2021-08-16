const express = require('express');
const router = express.Router();


router.get('/', (req, res, next) => {
    res.status(200).send({
        mensagem: 'GET - dentro de months'
    });
});

router.post('/', (req, res, next) => {
    res.status(201).send({
        mensagem: 'POST - dentro de months'
    });
});

module.exports = router;
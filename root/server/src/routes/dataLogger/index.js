//Get network interfaces and IPs
/* const { networkInterfaces } = require('os')
const nets = networkInterfaces()
console.log('Connected IP:', nets['en0'][1]['address']) */

const express = require('express')
const { format } = require('date-fns')
const dataLoggerData = require('../../models/dataLoggerModel')
const process = require('process')
require('dotenv').config()

const deviceApiKeys = process.env.DEVICES_API_KEYS
  ? process.env.DEVICES_API_KEYS.split(' ')
  : 'NO ENV VARIABLES FOUND'

const router = express.Router()

/** ---------------------------------- "/" --------------------------------------
 *
 *  - POST: new item
 *
 ** ---------------------------------------------------------------------------- */

router.route('/').post(validateDeviceApiKey, (req, res) => {
  if (req.isDeviceKeyValid) {
    const newData = new dataLoggerData(req.body['data'])
    newData
      .postItem()
      .then((savedItem) => {
        res.send(`Item posted: ${savedItem}`)
      })
      .catch((error) => {
        res.status(500).send(error)
      })
  } else {
    res.status(403).send(`You don't have permission to access this resource`)
  }
})

/**
 * ---------------------------------- "/:deviceName" --------------------------------------
 *  - GET: last item posted
 * -----------------------------------------------------------------------------
 */

router.route('/:deviceName').get(validateDevice, (req, res) => {
  if (req.deviceExist) {
    dataLoggerData
      .getLastItem(req.deviceName)
      .then((lastData) => res.send(lastData))
      .catch((error) => res.status(500).send(error))
  } else {
    res.send('Device not found')
  }
})

/**
 * --------------------------------- "/data" -----------------------------------
 *
 *  - GET: Array of items within the range of {startTime} and {endTime} params
 * -----------------------------------------------------------------------------
 */
router
  .route('/:deviceName/data')

  .get(validateDevice, (req, res) => {
    if (req.deviceExist) {
      const startTime = parseInt(req.query.startTime)
      const endTime = parseInt(req.query.endTime)
      dataLoggerData
        .getRangeOfItems(req.deviceName, startTime, endTime)
        .then((items) =>
          items.length > 0
            ? res.send(items)
            : res
                .status(404)
                .send('There is no data between the dates provided')
        )
        .catch(() => res.status(500).send('Error while finding data'))
    } else {
      res.send('Device not found')
    }
  })

  .delete(validateDevice, (req, res) => {
    if (req.deviceExist) {
      const unix_start = parseInt(req.query.startTime)
      const unix_end = parseInt(req.query.endTime)
      dataLoggerData
        .deleteItems(req.deviceName, unix_start, unix_end)
        .then((delData) => res.send({ 'Deleted data': delData }))
        .catch((error) => {
          res.status(500).send(`Error deleting items: ${error}`)
        })
    } else {
      res.send('Device not found')
    }
  })

/**
 * ---------------------------- "/data/latest-timestamp" ------------------------
 *
 *  - GET: String with the date of last item posted formated to "DD/MM/YY HH:mm"
 * -----------------------------------------------------------------------------
 */
router
  .route('/:deviceName/data/latest-timestamp')

  .get(validateDevice, (req, res) => {
    if (req.deviceExist) {
      dataLoggerData
        .getLastItem(req.deviceName)
        .then((lastItem) => {
          const date = new Date(lastItem['unix_time'] * 1000)
          const formattedDate = format(date, 'MMM dd, yyyy - HH:mm:ss')
          res.send({ date: formattedDate })
        })
        .catch(() => res.status(500).send('Error while getting the last item'))
    } else {
      res.send('Device not found')
    }
  })

/**
 * --------------------------- "/data/last/:nhours/hours" -----------------------
 *
 *  - GET: Array of items of the last {nhours}
 *  - limit {nhours} to 48 hours
 * ------------------------------------------------------------------------------
 */
router
  .route('/:deviceName/data/last/:nhours/hours')

  .get(validateDevice, async (req, res) => {
    if (req.deviceExist) {
      const nhours = req.nhours < 48 ? req.nhours : 1
      const nhours_unix_time = nhours * 3600

      const lastItem = await dataLoggerData
        .getLastItem(req.deviceName)
        .catch(() => console.log('Error while getting the last item'))

      const startTime = lastItem['unix_time'] - nhours_unix_time
      const endTime = lastItem['unix_time']
      dataLoggerData
        .getRangeOfItems(req.deviceName, startTime, endTime)
        .then((items) =>
          items.length > 0
            ? res.send(items)
            : res
                .status(404)
                .send(`There is no data in the last ${nhours} hours`)
        )
        .catch(() => res.status(500).send('Error while finding data'))
    } else {
      res.send('Device not found')
    }
  })

/**
 * --------------------------- "/data/last/:ndays/days" -----------------------
 *
 *  - GET: Array of items of the last {nhours}
 *  - limit {nhours} to 48 hours
 * ----------------------------------------------------------------------------
 */
router
  .route('/:deviceName/data/last/:ndays/days')

  .get(validateDevice, async (req, res) => {
    if (req.deviceExist) {
      const ndays = req.ndays <= 31 ? req.ndays : 1
      const ndays_unix_time = ndays * 3600 * 24

      const lastItem = await dataLoggerData
        .getLastItem(req.deviceName)
        .catch(() => console.log('Error while getting the last item'))

      const startTime = lastItem.unix_time - ndays_unix_time
      const endTime = lastItem.unix_time

      dataLoggerData
        .getRangeOfItems(req.deviceName, startTime, endTime)
        .then((items) =>
          items.length > 0
            ? res.send(items)
            : res.status(404).send(`There is no data in the last ${ndays} days`)
        )
        .catch(() => res.status(500).send('Error while finding data'))
    } else {
      res.send('Device not found')
    }
  })

/**
 * ---------------------------- "/data/:timestamp" -----------------------------
 *
 *  - GET: {timestamp} item -> should get closest item round to lower
 *  - DELETE: {timestamp} item -> needs exact timestamp
 *  - PUT: Update {timestamp} item -> needs exact timestamp
 * -----------------------------------------------------------------------------
 */
router
  .route('/:deviceName/data/:timestamp')

  .get(validateDevice, (req, res) => {
    if (req.deviceExist) {
      dataLoggerData
        .getOneItem(req.deviceName, req.timestamp)
        .then((item) => (item ? res.send(item) : res.send('Item not found')))
        .catch(() => res.status(500).send('Error finding item'))
    } else {
      res.send('Device not found')
    }
  })

  .put(validateDevice, (req, res) => {
    if (req.deviceExist) {
      dataLoggerData
        .updateOneItem(req.deviceName, req.timestamp, req.body)
        .then((item) =>
          item
            ? res.send(`Item updated: ${item}`)
            : res.status(404).send('Item not found to update')
        )
        .catch(() => res.status(500).send('Error while updating item'))
    } else {
      res.send('Device not found')
    }
  })

  .delete(validateDevice, (req, res) => {
    if (req.deviceExist) {
      dataLoggerData
        .deleteItem(req.deviceName, req.timestamp)
        .then((delData) => res.send(`Deleted data was:\n\n${delData}`))
        .catch((error) => {
          res.status(500).send(`Error deleting item: ${error}`)
        })
    } else {
      res.send('Device not found')
    }
  })

//-------------------------------------------------------------------------------
//---------------------- Middleware and parameters setting ----------------------
//-------------------------------------------------------------------------------
router.param('deviceName', (req, res, next) => {
  const deviceName = req.params.deviceName
  req.deviceName = deviceName
  next()
})

router.param('timestamp', (req, res, next) => {
  const time = parseInt(req.params.timestamp)
  req.timestamp = time
  next()
})

router.param('nhours', (req, res, next) => {
  const nhours = parseInt(req.params.nhours)
  req.nhours = nhours
  next()
})

router.param('ndays', (req, res, next) => {
  const ndays = parseInt(req.params.ndays)
  req.ndays = ndays
  next()
})

function validateDeviceApiKey(req, res, next) {
  const api_key = req.body['api_key']
  let isDeviceKeyValid = false
  if (deviceApiKeys.includes(api_key)) {
    isDeviceKeyValid = true
  }
  req.isDeviceKeyValid = isDeviceKeyValid
  next()
}

function validateDevice(req, res, next) {
  const validDevices = ['Logger_Dev', 'Logger_Hass', 'Logger_Avo']
  let deviceExist = false
  if (validDevices.includes(req.deviceName)) {
    deviceExist = true
  } else {
    deviceExist = false
  }
  req.deviceExist = deviceExist
  next()
}

//-------------------------------------------------------------------------------

module.exports = router
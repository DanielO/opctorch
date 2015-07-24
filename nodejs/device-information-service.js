var util = require('util');

var bleno = require('bleno');
var BlenoPrimaryService = bleno.PrimaryService;

var SerialNumberCharacteristic = require('./serial-number-characteristic');
var HardwareRevisionCharacteristic = require('./hardware-revision-characteristic');

function DeviceInformationService(opctorch) {
  DeviceInformationService.super_.call(this, {
    uuid: '180a',
    characteristics: [
      new SerialNumberCharacteristic(opctorch),
      new HardwareRevisionCharacteristic(opctorch)
    ]
  });
}

util.inherits(DeviceInformationService, BlenoPrimaryService);

module.exports = DeviceInformationService;

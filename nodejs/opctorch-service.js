var util = require('util');

var bleno = require('bleno');
var BlenoPrimaryService = bleno.PrimaryService;

var OPCTorchMessageRGBCharacteristic = require('./opctorch-message-rgb-characteristic');
var OPCTorchFlameRGBCharacteristic = require('./opctorch-flame-rgb-characteristic');
var OPCTorchBrightnessCharacteristic = require('./opctorch-brightness-characteristic');
var OPCTorchMessageCharacteristic = require('./opctorch-message-characteristic');

function OPCTorchService(opctorch) {
  OPCTorchService.super_.call(this, {
    uuid: '69b9ac8b-2f23-471a-ba62-0e3c9e8c0000',
    characteristics: [
      new OPCTorchMessageRGBCharacteristic(opctorch),
      new OPCTorchFlameRGBCharacteristic(opctorch),
      new OPCTorchBrightnessCharacteristic(opctorch),
      new OPCTorchMessageCharacteristic(opctorch),
    ]
  });
}

util.inherits(OPCTorchService, BlenoPrimaryService);

module.exports = OPCTorchService;

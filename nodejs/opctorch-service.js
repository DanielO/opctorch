var util = require('util');

var bleno = require('bleno');
var BlenoPrimaryService = bleno.PrimaryService;

var OPCTorchRGBCharacteristic = require('./opctorch-rgb-characteristic');

function OPCTorchService(opctorch) {
  OPCTorchService.super_.call(this, {
    uuid: '01010101010101010101010101010101',
    characteristics: [
      new OPCTorchRGBCharacteristic(opctorch),
    ]
  });
}

util.inherits(OPCTorchService, BlenoPrimaryService);

module.exports = OPCTorchService;

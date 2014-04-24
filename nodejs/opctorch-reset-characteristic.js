var util = require('util');

var bleno = require('bleno');
var BlenoCharacteristic = bleno.Characteristic;
var BlenoDescriptor = bleno.Descriptor;

function OPCTorchResetCharacteristic(opctorch) {
  OPCTorchResetCharacteristic.super_.call(this, {
    uuid: '69b9ac8b-2f23-471a-ba62-0e3c9e8c0005',
    properties: ['write', 'writeWithoutResponse'],
    descriptors: [
      new BlenoDescriptor({
        uuid: '2901',
        value: 'reset opctorch settings'
      })
    ]
  });

  this.opctorch = opctorch;
}

util.inherits(OPCTorchResetCharacteristic, BlenoCharacteristic);

OPCTorchResetCharacteristic.prototype.onWriteRequest = function(data, offset, withoutResponse, callback) {
  if (offset) {
    callback(this.RESULT_ATTR_NOT_LONG);
  } else {
    console.log("Reset characteristic");
    this.opctorch.cmd("reset");
    callback(this.RESULT_SUCCESS);
  }
};

module.exports = OPCTorchResetCharacteristic;

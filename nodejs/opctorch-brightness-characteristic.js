var util = require('util');

var bleno = require('bleno');
var BlenoCharacteristic = bleno.Characteristic;
var BlenoDescriptor = bleno.Descriptor;

function OPCTorchBrightnessCharacteristic(opctorch) {
  OPCTorchBrightnessCharacteristic.super_.call(this, {
    uuid: '69b9ac8b-2f23-471a-ba62-0e3c9e8c0001',
    properties: ['write', 'writeWithoutResponse'],
    descriptors: [
      new BlenoDescriptor({
        uuid: '2901',
        value: 'set opctorch brightness value'
      })
    ]
  });

  this.opctorch = opctorch;
}

util.inherits(OPCTorchBrightnessCharacteristic, BlenoCharacteristic);

OPCTorchBrightnessCharacteristic.prototype.onWriteRequest = function(data, offset, withoutResponse, callback) {
  if (offset) {
    callback(this.RESULT_ATTR_NOT_LONG);
  } else if (data.length !== 1) {
    callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
  } else {
    var b = data.readUInt8(0);
    console.log("Brightness characteristic " + b);
    this.opctorch.set("brightness", b);
    callback(this.RESULT_SUCCESS);
  }
};

module.exports = OPCTorchBrightnessCharacteristic;

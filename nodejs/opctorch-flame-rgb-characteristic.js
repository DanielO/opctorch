var util = require('util');

var bleno = require('bleno');
var BlenoCharacteristic = bleno.Characteristic;
var BlenoDescriptor = bleno.Descriptor;

function OPCTorchFlameRGBCharacteristic(opctorch) {
  OPCTorchFlameRGBCharacteristic.super_.call(this, {
    uuid: '69b9ac8b-2f23-471a-ba62-0e3c9e8c0004',
    properties: ['write', 'writeWithoutResponse'],
    descriptors: [
      new BlenoDescriptor({
        uuid: '2901',
        value: 'set opctorch flame RGB value'
      })
    ]
  });

  this.opctorch = opctorch;
}

util.inherits(OPCTorchFlameRGBCharacteristic, BlenoCharacteristic);

OPCTorchFlameRGBCharacteristic.prototype.onWriteRequest = function(data, offset, withoutResponse, callback) {
  if (offset) {
    callback(this.RESULT_ATTR_NOT_LONG);
  } else if (data.length !== 3) {
    callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
  } else {
    var r = data.readUInt8(0);
    var g = data.readUInt8(1);
    var b = data.readUInt8(2);
    console.log("Flame RGB characteristic " + r + " " + g + " " + b);
    this.opctorch.set("red_energy", r);
    this.opctorch.set("green_energy", g);
    this.opctorch.set("blue_energy", b);
    callback(this.RESULT_SUCCESS);
  }
};

module.exports = OPCTorchFlameRGBCharacteristic;

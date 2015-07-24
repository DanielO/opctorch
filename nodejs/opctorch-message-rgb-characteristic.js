var util = require('util');

var bleno = require('bleno');
var BlenoCharacteristic = bleno.Characteristic;
var BlenoDescriptor = bleno.Descriptor;

function OPCTorchMessageRGBCharacteristic(opctorch) {
  OPCTorchMessageRGBCharacteristic.super_.call(this, {
    uuid: '69b9ac8b-2f23-471a-ba62-0e3c9e8c0003',
    properties: ['write', 'writeWithoutResponse'],
    descriptors: [
      new BlenoDescriptor({
        uuid: '2901',
        value: 'set opctorch message RGB value'
      })
    ]
  });

  this.opctorch = opctorch;
}

util.inherits(OPCTorchMessageRGBCharacteristic, BlenoCharacteristic);

OPCTorchMessageRGBCharacteristic.prototype.onWriteRequest = function(data, offset, withoutResponse, callback) {
  if (offset) {
    callback(this.RESULT_ATTR_NOT_LONG);
  } else if (data.length !== 3) {
    callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
  } else {
    var r = data.readUInt8(0);
    var g = data.readUInt8(1);
    var b = data.readUInt8(2);
    console.log("Message RGB characteristic " + r + " " + g + " " + b);
    this.opctorch.set("text_red", r);
    this.opctorch.set("text_green", g);
    this.opctorch.set("text_blue", b);
    callback(this.RESULT_SUCCESS);
  }
};

module.exports = OPCTorchMessageRGBCharacteristic;

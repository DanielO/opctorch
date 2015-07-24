var util = require('util');

var bleno = require('bleno');
var BlenoCharacteristic = bleno.Characteristic;
var BlenoDescriptor = bleno.Descriptor;

function OPCTorchRGBCharacteristic(opctorch) {
  OPCTorchRGBCharacteristic.super_.call(this, {
    uuid: '01010101010101010101010101524742',
    properties: ['write', 'writeWithoutResponse'],
    descriptors: [
      new BlenoDescriptor({
        uuid: '2901',
        value: 'set opctorch RGB value'
      })
    ]
  });

  this.opctorch = opctorch;
}

util.inherits(OPCTorchRGBCharacteristic, BlenoCharacteristic);

OPCTorchRGBCharacteristic.prototype.onWriteRequest = function(data, offset, withoutResponse, callback) {
  if (offset) {
    callback(this.RESULT_ATTR_NOT_LONG);
  } else if (data.length !== 3) {
    callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
  } else {
    var r = data.readUInt8(0);
    var g = data.readUInt8(1);
    var b = data.readUInt8(2);
    console.log("RGB characteristic " + r + " " + g + " " + b);
    callback(this.RESULT_SUCCESS);
/*
     this.opctorch.setRGB(r, g, b, function() {
      callback(this.RESULT_SUCCESS);
    }.bind(this));
*/
  }
};

module.exports = OPCTorchRGBCharacteristic;

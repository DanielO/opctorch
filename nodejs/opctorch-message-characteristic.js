var util = require('util');

var bleno = require('bleno');
var BlenoCharacteristic = bleno.Characteristic;
var BlenoDescriptor = bleno.Descriptor;

function OPCTorchMessageCharacteristic(opctorch) {
  OPCTorchMessageCharacteristic.super_.call(this, {
    uuid: '69b9ac8b-2f23-471a-ba62-0e3c9e8c0002',
    properties: ['write', 'writeWithoutResponse'],
    descriptors: [
      new BlenoDescriptor({
        uuid: '2901',
        value: 'set opctorch message'
      })
    ]
  });

  this.opctorch = opctorch;
}

util.inherits(OPCTorchMessageCharacteristic, BlenoCharacteristic);

OPCTorchMessageCharacteristic.prototype.onWriteRequest = function(data, offset, withoutResponse, callback) {
  if (offset) {
    callback(this.RESULT_ATTR_NOT_LONG);
  } else {
    var i;
    for (i = 0; i < data.length; i++) {
      var d = data.readUInt8(i);
      if (d < 32 || d > 126) {
	console.log("character out of bounds");
	callback(this.RESULT_UNLIKELY_ERROR);
	return;
      }
    }
    var s = data.toString('ascii');
    console.log("Message characteristic: " + s);
    this.opctorch.cmd("message " + s);
    callback(this.RESULT_SUCCESS);
  }
};

module.exports = OPCTorchMessageCharacteristic;

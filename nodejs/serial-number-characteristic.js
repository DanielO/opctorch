var util = require('util');

var bleno = require('bleno');

var BlenoCharacteristic = bleno.Characteristic;
var BlenoDescriptor = bleno.Descriptor;

function SerialNumberCharacteristic(opctorch) {
  SerialNumberCharacteristic.super_.call(this, {
    uuid: '2a25',
    properties: ['read'],
    descriptors: [
      new BlenoDescriptor({
        uuid: '2901',
        value: 'opctorch serial number'
      })
    ]
  });

  this.opctorch = opctorch;
}

util.inherits(SerialNumberCharacteristic, BlenoCharacteristic);

SerialNumberCharacteristic.prototype.onReadRequest = function(offset, callback) {
  if (offset) {
    callback(this.RESULT_ATTR_NOT_LONG, null);
  } else {
    this.opctorch.serialNumber(function(serial) {
      callback(this.RESULT_SUCCESS, new Buffer(serial));
    }.bind(this));
  }
};

module.exports = SerialNumberCharacteristic;

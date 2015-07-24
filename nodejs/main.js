var bleno = require('bleno');

var DeviceInformationService = require('./device-information-service');
var OPCTorchService = require('./opctorch-service');

var OPCTorch = require('./opctorch');
var opctorch = new OPCTorch();

var deviceInformationService = new DeviceInformationService(opctorch);
var opcTorchService = new OPCTorchService(opctorch);

bleno.on('stateChange', function(state) {
  console.log('on -> stateChange: ' + state);

  if (state === 'poweredOn') {
    bleno.startAdvertising('opctorch', [opcTorchService.uuid]);
  } else {
    bleno.stopAdvertising();
  }
});

bleno.on('advertisingStart', function(error) {
  console.log('on -> advertisingStart: ' + (error ? 'error ' + error : 'success'));

  if (!error) {
      bleno.setServices([
        deviceInformationService,
        opcTorchService
    ]);
  }
});

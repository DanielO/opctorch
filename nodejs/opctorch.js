
var OPCTorch = function(value1, value2) {
  this.value1 = value1;
}

OPCTorch.prototype.version = function(fn) {
  var v = "1.0";
  fn(v);
};

OPCTorch.prototype.serialNumber = function(fn) {
    fn("000001");
};

module.exports = OPCTorch;

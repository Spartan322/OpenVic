## Events are exclusively for the purpose of handling global signals
## This is to reduce "signal bubbling" which is when a signal callback is used to "bubble" the signal callbacks up the scene tree.
## It does such by providing a global interface of signals that are connected to and emitted by that are garunteed to exist.
extends Node

var Loader: LoaderEventsObject = LoaderEventsObject.new()
var Options: OptionsEventsObject = OptionsEventsObject.new()

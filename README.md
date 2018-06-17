# CLI Modular Audio Tools

## Getting Started

These tools use `jack` for access to sound hardware and routing. On my Ubuntu 
16.04i `jackd2` seemed to work out of the box, `jackd1` needed some extra help.

### jackd1

Install `libjack-dev`

Add the following to `/etc/security/limits.conf`

```
@audio           -       rtprio          99
```

Add your user to the `audio` group

```
sudo usermod -a -G audio $(whoami)
```

## Jack Usage

1. create a `client` connection to the jackd service `jack_client_open`
2. register callbacks using the `client`
    1. `jack_set_process_callback`
    2. `jack_on_shutdown`
3. register input and/or output ports `jack_port_register`
4. activate the client `jack_activate`
5. (optionally) connect to the hardware outputs `jack_get_ports`,
 `jack_connect` and `jack_free`. The client **MUST** be activated at this point
6. sleep
7. close the jack client down `jack_client_close`

## TODO

* f̶i̶n̶d̶ ̶o̶u̶t̶ ̶w̶h̶y̶ ̶s̶i̶m̶p̶l̶e̶ ̶c̶l̶i̶e̶n̶t̶ ̶f̶r̶o̶m̶ ̶j̶a̶c̶k̶2̶ ̶p̶l̶a̶y̶s̶ ̶a̶ ̶s̶o̶u̶n̶d̶ ̶w̶h̶e̶r̶e̶a̶s̶ ̶t̶h̶e̶ ̶m̶i̶d̶i̶s̶i̶n̶e̶ ̶
n̶e̶e̶d̶s̶ ̶t̶o̶ ̶b̶e̶ ̶p̶a̶t̶c̶h̶e̶d̶ ̶t̶o̶ ̶t̶h̶e̶ ̶m̶a̶i̶n̶ ̶o̶u̶t̶  `jack_connect` is called

* get a port registration callback working - we need to dynamically hook up
ports to allow circular modulation paths. Some ports may not exist in a devices
modulation config when it is created but may become available later. We can also
offer somthing like jack_connect but this is slighty more cumbersome than the
proposed UI:

## API

```
osc -n lfo -f 2         # use an oscilator as a 2hz lfo
osc -f 200 -f lfo:1     # modulate a 200hz oscilators freq using the 2hz lfo
```

| gen    |                              |
|--------|------------------------------|-
| osc    | mode, freq, sync, pw         | audio
| adsr   | a, d, s, r, loop             | control
| filter | mode, freq, res, drive       | audio
| envfol | sensitivity                  | control
| seq    | 
| gate   |

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

* check how the port metadata system works so that we can pass additional info 
with ports such as default values for modulation amplitude (eg. a keyboard
device where amplitude 1 means keyfollow)
* fix up makefile so it does not build all of the lib folder

## API

```
osc -n lfo -f 2         # use an oscilator as a 2hz lfo
osc -f 200 -f lfo:1     # modulate a 200hz oscilators freq using the 2hz lfo
```

| gen    | parameters                   | type    |
|--------|------------------------------|---------|
| osc    | mode, freq, sync, pw         | audio   |
| adsr   | a, d, s, r, loop             | control |
| filter | mode, freq, res, drive       | audio   |
| envfol | sensitivity                  | control |
| seq    | tbc                          | tbc     |
| gate   | tbc                          | tbc     |

#### Parameters

Paramters can be set to a static value or modulated by multiple sources or a
combination of both. An example of an oscillator set to 220hz modulated by an
lfo and a sequencer.

```shell
# a 1hz oscillator named 'lfo'
osc lfo -f 1 &

# a sequencer named 'notes' which steps each time 'lfo' crosses 0 (60bpm)
cat notes.txt | seq notes -t lfo:0 &

# an oscillator tuned to 220hz modulated by an lfo and a sequencer
osc -f 220 -f lfo:0.01 -f seq:1 -p
```

##### Syntax

client              = string
port                = string
value               = float

value               = float
client              = string
client:value        = string:float
client:port         = string:string
client:port:value   = string:string:float


### Alternative Syntax?

how could we do this? what does stdout produce?

stdout could produce config in the form of text. we could add a shebang to the 
config.

we could use bitfmt_misc to make executables

how does osc fork in the sample below.
```
osc -f 1 > lfo
lfo | seq
lfo | osc
```

## OCS TODO

1. add extra outputs for the other wavesforms
1. make clis automatically connect all outputs to hardware inputs if the play
   flag is passed.
1. add a callback for connected ports - use it to render samples to the
   connected outputs and skip the disconnected ones

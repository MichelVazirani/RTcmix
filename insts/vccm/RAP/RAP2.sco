/* RECORD:  p0 = start; p1 = duration (-endtime); p2 = inchan; p3 = audio index */
/* PLAY:  p0 = outsk; p1 = insk; p2 = duration (-endtime); p3 = amp; p4 = aud_idx */

load("iRAP")
set_option("full_duplex_on")
rtsetparams(44100, 2, 512)
rtinput("AUDIO")
xrtinput("/snd/Public_Sounds/train1.mix.aif")
bus_config("RECORD", "in0-1","aux0out")

RECORD(0,5,0,0)
setline(0,0, 10, 1, 11, 0)
PLAY(4, 0, 5, 1, 0)
PLAY(5, 0, 3, 1, 0)



local lm = require "luamake"

lm:rule "run_luamake" {
    args = {
        "$luamake",
        "-mode", lm.mode,
        "-C", lm.workdir,
        "-f", "$in",
        "$args",
    },
    pool = "console",
}

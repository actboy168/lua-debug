local rdebug = require 'remotedebug.visitor'
local ev = require 'backend.event'
ev.on('updateVmState', function(reason)
	-- should update ngx time, because ngx time is a cache value
	local ngx = rdebug.fieldv(rdebug._G, 'ngx')
	if not ngx then return end

	local update_time = rdebug.fieldv(ngx, "update_time")
	if not update_time then return end

	rdebug.eval(update_time)
end)

local content = ...
local f = load(content, "=eval.dump")
if not f then
    return
end
return string.dump(f)

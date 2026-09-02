local content = ...
local f, err = load(content, "=eval.dump")
if not f then
    error(err, 0)
end
return string.dump(f)

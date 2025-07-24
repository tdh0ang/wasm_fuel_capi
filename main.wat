(module
  (func (export "main")
    (local $i i32)
    (local.set $i (i32.const 0))
    (loop $loop
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.lt_s (local.get $i) (i32.const 50000)))
    )
  )
)

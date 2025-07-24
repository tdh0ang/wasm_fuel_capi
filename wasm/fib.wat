(module
  (func $main (param $n i32) (result i32)
    (if
      (i32.lt_s (local.get $n) (i32.const 2))
      (then (return (local.get $n)))
    )
    (i32.add
      (call $main (i32.sub (local.get $n) (i32.const 1)))
      (call $main (i32.sub (local.get $n) (i32.const 2)))
    )
  )
  (export "main" (func $main))
)
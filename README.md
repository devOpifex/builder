<div align="center">
  <h1>Builder</h1>
  <a href="https://builder.opifex.org">docs</a>
</div>

A simple preprocessor for R: use macros, directives, conditionals, and more to generate your code.

Only works on POSIX systems.

```bash
curl -fsSL https://builder.opifex.org/install.sh | sh
```

## Example

The following source code

```r
#> define APP_NAME "Speed Analyzer"
#> define THRESHOLD 15

#> include:csv sql/speeds.csv cars

#> macro
DEBUG <- function(var) {
  cat(..var, " = ", .var, "\n", sep = "")
}
#> endmacro

get_stats <- function(x) {
  list(min(x), max(x), mean(x))
}

.[lo, hi, avg] <- get_stats(cars$speed)

print(..FMT("{APP_NAME}: speeds from {lo} to {hi}"))
print(..FMT("avg ({avg}) exceeds threshold: {avg > THRESHOLD}"))

DEBUG(lo)
DEBUG(hi)
```

Gets expanded to the code below.

```r
cars <- structure(
  list(speed = c(4L, 10L, 15L, 20L, 25L), dist = c(2L, 26L, 54L, 48L, 85L)),
  class = "data.frame",
  row.names = c(NA, -5L)
)
get_stats <- function(x) {
  list(min(x), max(x), mean(x))
}
.destructure_tmp_ <- get_stats(cars$speed)
lo <- .destructure_tmp_[[1]]
hi <- .destructure_tmp_[[2]]
avg <- .destructure_tmp_[[3]]
print(sprintf("%s: speeds from %s to %s", "Speed Analyzer", lo, hi))
print(sprintf("avg (%s) exceeds threshold: %s", avg, avg > 15))
cat("lo", " = ", lo, "\n", sep = "")
cat("hi", " = ", hi, "\n", sep = "")
```

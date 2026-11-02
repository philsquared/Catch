<a id="top"></a>
# How to run specific section/generator

> The generator and generic path filtering was added in Catch2 3.13.0

Catch2 supports picking specific path through a test case by filtering
sections and generator indices to run through. This is done by using one
of the three commandline parameters, one or more times.

```
-c, --section           <section name>
-g, --generator-index   <index spec>
-p, --path-filter       <path filter spec>
```

All the variants form a shared stack of filters, but if you use only
`-c`/`--section` form to specify section filters, you will get the old
behaviour, which does not affect generators at all. If you also use either
`-g`/`--generator-index`, or `-p`/`--path-filter`, you will get the new
behaviour, which can also filter generator elements.

Both the new and old filter behaviours include some potentially surprising
things:
  * Code outside of sections being skipped will still be executed. E.g.
    any setup code in the TEST_CASE that lives outside of sections.
  * Path filters filter the prefix of the path. So if you specify single
    filter, it affects only the top level sections/generator, with their
    child sections/generators being unfiltered.
  * Path filters are independent of test case selection, Catch2 will try
    to follow the path filters in all selected test cases. This means
    that if you specify path filters without a test case filter, Catch2
    will try to apply the path filters inside every registered test case.

## Old behaviour

> The old behaviour was deprecated in Catch2 3.13.0

```
-c, --section           <section name>
```
The argument to `-c`/`--section` can be any arbitrary string. When Catch2
is deciding whether to enter a section, it will check its trimmed name
against the appropriate trimmed section filter. If they are the same,
the section can be opened. If not, Catch2 will skip over that section.


### Examples

#### Simple section nesting
Given
```cpp
TEST_CASE( "foo" ) {
    REQUIRE( true );
    SECTION( "A" ) {
        SECTION( "A1" ) { REQUIRE( true ); }
        SECTION( "A2" ) { REQUIRE( true ); }
    }
    SECTION( "B" ) {
        SECTION( "B1" ) { REQUIRE( true ); }
        SECTION( "B2" ) { REQUIRE( true ); }
    }
}
```

* `./tests foo -c A` runs section "A" and both of its subsections,
   resulting in 4 assertions.
* `./tests foo -c A -c B` runs section "A", but none of its subsections,
   resulting in 1 assertion (the one before "A").
* `./tests foo -c A -c A1` runs section "A" and only the "A1" subsection,
   resulting in 2 assertions.


#### Sections with nested generators
Note that old behaviour completely _ignores_ generators. This means both
that they can't be filtered, but also that they aren't taken into account
for the filter depth. In other words, given
```cpp
TEST_CASE( "bar" ) {
    REQUIRE( true );
    SECTION( "A" ) { REQUIRE( true ); }
    SECTION( "B" ) {
        auto i = GENERATE( 1, 2, 3 );
        DYNAMIC_SECTION( "i=" << i ) {
            REQUIRE( true );
        }
    }
}
```
* `./tests bar -c A` results in 2 assertions.
* `./tests bar -c B -c i=2` results in 4 assertions, because the whole
  generator in section "B" has to be used up, but the dynamic section is
  only entered when the generator returns 2 as the value for `i`.
* `./tests bar -c B -c i=4` results in 3 assertions, because the assertion
  outside of section is executed every time the test case is entered, and
  the generator forces the test case to rerun 3 times before it is used up,
  even though the dynamic section will never be entered.


#### Section with sibling generators
For cases where sections have sibling generators, the filtering can get
even more surprising.
```cpp
TEST_CASE( "qux" ) {
    REQUIRE( true );
    SECTION( "A" ) { REQUIRE( true ); }
    auto i = GENERATE( 1, 2, 3 );
    DYNAMIC_SECTION( "i=" << i ) {
        REQUIRE( true );
    }
}
```
* `./tests qux -c A` results in **4** assertions, because section "A" is
  entered once, but the sibling generator has to be exhausted, and
  the first assertion is executed once per generator element.
* `./tests qux -c i=2` also results in 4 assertions. Once again,
  the generator has to be exhausted and the dynamic section is entered
  once.


## New behaviour

> The new behaviour was introduced in Catch2 3.13.0

```
-g, --generator-index   <index spec>
-p, --path-filter       <path filter spec>
```

The argument to `-g`/`--generator-index` can be a non-negative index,
a comma-separated list of indices, inclusive ranges, or "\*", which allows
all elements from the generator. Indices are zero-based.

> Lists and ranges were [introduced](https://github.com/catchorg/Catch2/issues/3071) in Catch2 X.Y.Z

For example, `-g 1,3,6-10` selects indices 1, 3, and 6 through 10 (including
both endpoints). `-g 1-1` selects just index 1. Each index is visited once,
in the specified order. Each selected element runs all matching child
sections and generators before moving to the next selected element.

Indices and ranges must be in increasing order and must not overlap.
Duplicate indices, decreasing ranges, and malformed selections are errors:
`1,2,2`, `3,1`, `1-3,2-4`, `3-1`, and `1--2` are rejected. Open-ended
ranges such as `3-*` and mixtures of "\*" with indices are not supported.
Whitespace around indices and range endpoints is allowed.

A list or range belongs to one generator. Repeating `-g` adds filters for
successive generators along the execution path, as before.

Providing an index outside of the generator is an error, including when
earlier selected indices were valid.

The argument to `-p`/`--path-filter` must start with either "c:" for
a section filter, or with "g:" for a generator filter. Everything past
the colon is then parsed as either a section filter, or a generator filter.

Note that using `p`/`--path-filter` enables new filtering behaviour, even
if it is only used to add section filters.

There is another important difference between filtering out sections and
generators. A section can be left un-entered, but a generator always has
to be active. For this reason, if generator fails a filter
(e.g. there is a section filter at given depth instead), it has to stop
the execution of the test case. Currently, this is done via `SKIP()`
equivalent, causing the section to be considered skipped.


### Examples

#### Nested generators
```cpp
TEST_CASE( "waldo" ) {
    auto i = GENERATE( 1, 10, 100 );
    auto j = GENERATE( 2, 20, 200 );
    CAPTURE( i, j );
    REQUIRE( true );
}
```
* `./tests waldo -g 1` results in 3 assertions, with `i := 10`, because
  the second nested generator is unfiltered.
* `./tests waldo -g 1 -g 2` results in 1 assertion, with `i := 10, j := 200`.
* `./tests waldo -g * -g 2` results in 3 assertions, all with `j := 200`.
* `./tests waldo -g 1 -g *` results in 3 assertions, all with `i := 10`.
* `./tests waldo -g 3` results in 1 **failed** assertion, because the first
  generator does not have 3rd element.
* `./tests waldo -g * -g 3` results in 3 **failed** assertions, as the
  second generator does not have 3rd element, but we have to exhaust the
  first generator.

#### Generator with a nested dynamic section
```cpp
TEST_CASE( "grault" ) {
    REQUIRE( true );
    auto i = GENERATE( 1, 2, 3 );
    DYNAMIC_SECTION( "i=" << i ) {
        REQUIRE( true );
    }
}
```
* `./tests grault -p g:1` results in 2 assertions, as there is no filter
  on the dynamic section.
* `./tests grault -p g:1 -p c:i=2` results in 2 assertions, as the filter
  on the dynamic section matches the element given from the generator.
* `./tests grault -p g:1 -p c:i=3` results in 1 assertion, as the generator
  is limited to only try `i := 2` and the dynamic section is filtered out.

#### Section with a sibling generator
Because generators have to stop test execution when they don't pass filter,
it is impossible to run only a section with sibling generator without
triggering a test case skip. Consider this test case from an earlier example:
```cpp
TEST_CASE( "qux" ) {
    REQUIRE( true );
    SECTION( "A" ) { REQUIRE( true ); }
    auto i = GENERATE( 1, 2, 3 );
    DYNAMIC_SECTION( "i=" << i ) {
        REQUIRE( true );
    }
}
```
* `./tests qux -p g:1` results in 2 assertions, as the dynamic section is
  entered only once.
* `./tests qux -p g:1 -p c:i=1` results in 1 assertion, as the dynamic
  section filter is incompatible with the generator filter.
* `./tests qux -p c:A` results in 2 assertions **and a skipped test case**.
  This is because the generator is sibling to section "A", and thus reads
  the same section filter. However, it is not a section and as thus cannot
  proceed.
* `./tests qux -p c:i=2` results in 1 assertion **and a skipped test case**.
  Once again, the first filter in the filter stack is a section filter,
  and thus the generator cannot proceed.

Compare this with the old filter behaviour, where `./tests qux -c i=2`
would instead result in 4 assertions, because the generator would go
through all elements.


---

[Home](Readme.md#top)

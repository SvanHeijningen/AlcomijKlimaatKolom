drop table if exists setting;

WITH  settings (span,                  age)
AS (VALUES 
               (interval '15 minutes', interval '30 days'))
			   
SELECT span, age, 
cast(1000 * extract(epoch from (now() - age)) as bigint) as max_ts, 
cast(1000 * extract(epoch from span) as bigint) as span_ticks 
INTO temporary table setting FROM settings;

drop table if exists public.aggregated_ts_kv;

SELECT entity_type, entity_id, key, ts / span_ticks * span_ticks as ts, avg(dbl_v) as dbl_v, count(dbl_v) as cnt
into public.aggregated_ts_kv
	FROM public.ts_kv
	join setting on 1=1
	where ts < max_ts
	group by span_ticks, max_ts, entity_type, entity_id, key, ts / span_ticks
	order by ts
	;
	
select count(*) as count_aggregated_values, sum(cnt) as count_source_values
from public.aggregated_ts_kv
;
delete 
	FROM public.ts_kv
	where ts < (select max_ts from setting)
;

insert INTO public.ts_kv (entity_type, entity_id, key, ts, dbl_v)
select entity_type, entity_id, key, ts, dbl_v
from public.aggregated_ts_kv
;
drop table if exists public.aggregated_ts_kv;
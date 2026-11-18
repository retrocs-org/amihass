"""Offline transport design experiment; not a live bridge transport."""
import json,re
MAX_SELECTED=48

def subscription(entity_ids,request_id=1):
    selected=sorted(set(entity_ids))
    if not selected:
        return None  # Never send an unfiltered subscription.
    if len(selected)>MAX_SELECTED or not isinstance(request_id,int) or request_id<1:
        raise ValueError('Selected-entity or request limit exceeded')
    if any(len(e)>95 or not re.fullmatch(r'[a-z0-9_]+\.[a-z0-9_]+',e) for e in selected):
        raise ValueError('Invalid entity ID')
    return {'id':request_id,'type':'subscribe_entities','entity_ids':selected}

def measurements():
    result=[]
    for count in (1,6,24,48):
        frame=json.dumps(subscription([f'light.design_{i}' for i in range(count)]),separators=(',',':')).encode()
        result.append({'selected_entities':count,'subscription_bytes':len(frame),'poll_requests_per_minute_at_30s':count*2})
    return result

if __name__=='__main__':
    print(json.dumps({'mode':'offline design only','scenarios':measurements()},indent=2))

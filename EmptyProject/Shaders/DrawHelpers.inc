
// http://jcgt.org/published/0003/04/01/paper.pdf
// Computes the signed distance from a line
float line_distance(float2 p, float2 p1, float2 p2) 
{
	float2 center = (p1 + p2) * 0.5;
	float len = length(p2 - p1);
	float2 dir = (p2 - p1) / len;
	float2 rel_p = p - center;
	return dot(rel_p, float2(dir.y, -dir.x));
}

// http://jcgt.org/published/0003/04/01/paper.pdf
// Computes the signed distance from a line segment
float segment_distance(float2 p, float2 p1, float2 p2) 
{
	float2 center = (p1 + p2) * 0.5;
	float len = length(p2 - p1);
	float2 dir = (p2 - p1) / len;
	float2 rel_p = p - center;
	float dist1 = abs(dot(rel_p, float2(dir.y, -dir.x)));
	float dist2 = abs(dot(rel_p, dir)) - 0.5*len;
	return max(dist1, dist2);
}

// http://jcgt.org/published/0003/04/01/paper.pdf
float arrow_angle(float2 texcoord, float body, float head, float height, float linewidth, float antialias)
{
	float d;
	float w = linewidth/2.0 + antialias;
	float2 start = float2(body/2.0, 0.0);
	float2 end = float2(body/2.0, 0.0);
	// Arrow tip (beyond segment end)
	if( texcoord.x > body/2.0) {
		// Head : 2 segments
		float d1 = line_distance(texcoord,
		end, end - head * float2(+1.0,-height));
		float d2 = line_distance(texcoord,
		end - head*float2(+1.0,+height), end);
		// Body : 1 segment
		float d3 = end.x - texcoord.x;
		d = max(max(d1,d2), d3);
	} else {
		// Head : 2 segments
		float d1 = segment_distance(texcoord,
		end - head*float2(+1.0,-height), end);
		float d2 = segment_distance(texcoord,
		end - head*float2(+1.0,+height), end);
		// Body : 1 segment
		float d3 = segment_distance(texcoord, start, end - float2(linewidth,0.0));
		d = min(min(d1,d2), d3);
	}
	return d;
}

float4 filled(float distance, // Signed distance to line
	float linewidth, // Stroke line width
	float antialias, // Stroke antialiased area
	float4 fill) // Fill color
{
	float t = linewidth / 2.0 - antialias;
	float signed_distance = distance;
	float border_distance = abs(signed_distance) - t;
	float alpha = border_distance / antialias;
	alpha = exp(-alpha * alpha);
	if( border_distance < 0.0 )
		return fill;
	else if( signed_distance < 0.0 )
		return fill;
	else
		return float4(fill.rgb, alpha * fill.a);
}


float4 HelperArrow(float2 ScreenCoord, float2 ScreenPositionStart, float2 ScreenPositionEnd, float4 InColor) {
	float2 R = ScreenPositionEnd - ScreenPositionStart;
	float L = length(R);
	R = R / L;
	L = max(0.f, L - 0.75f);

	ScreenPositionStart -= 0.75f * R;
	ScreenPositionEnd -= 0.75f * R;

	// R = [ cos, sin ]
	float2 Position = (ScreenCoord - ScreenPositionEnd);
	Position = float2(Position.x * R.x + Position.y * R.y, Position.x * -R.y + Position.y * R.x);

	float4 OutColor = filled(
		arrow_angle(Position, 1.f, 2.f, 1.00, L, 0.5f), 
		1.f, 
		0.5f, 
		InColor);
	OutColor.xyz *= OutColor.w;
	return OutColor;
}